import { sendWithPromise } from 'chrome://resources/js/cr.js';
import initOpaque, { login_start } from '/opaque_wasm.js';

const AUTH_ORIGIN = 'https://auth.anar.one';
const ACCOUNT_ORIGIN = 'https://account.anar.one';
const KEYRING_AAD = new TextEncoder().encode('anar-keyring-v1');
const IV_BYTES = 12;

const ANAR_SERVICES = [
  { id: 'vault', name: 'Anar Vault', url: 'https://vault.anar.one' },
  { id: 'notebook', name: 'Anar Notebook', url: 'https://note.anar.one' },
  { id: 'mail', name: 'Anar Mail', url: 'https://mail.anar.one' },
  { id: 'calendar', name: 'Anar Calendar', url: 'https://calendar.anar.one' },
  { id: 'tarrah', name: 'Anar Tarrah', url: 'https://tarrah.anar.one' },
  { id: 'voicer', name: 'Anar Voicer', url: 'https://voicer.anar.one' },
];

const el = (id) => document.getElementById(id);

let opaqueReady = null;
let pendingOtp = null;
let addingAccount = false;
let reconnecting = false;

function ensureOpaque() {
  if (!opaqueReady) {
    opaqueReady = sendWithPromise('anarAccountGetOpaqueWasm').then((encoded) =>
      initOpaque(base64ToBytes(encoded)),
    );
  }
  return opaqueReady;
}

function bytesToBase64(bytes) {
  let bin = '';
  for (let i = 0; i < bytes.length; i++) {
    bin += String.fromCharCode(bytes[i]);
  }
  return btoa(bin);
}

function base64ToBytes(b64) {
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) {
    out[i] = bin.charCodeAt(i);
  }
  return out;
}

async function requesterPublicKey() {
  const pair = await crypto.subtle.generateKey(
    { name: 'ECDH', namedCurve: 'P-256' }, true, ['deriveBits'],
  );
  return bytesToBase64(new Uint8Array(await crypto.subtle.exportKey('raw', pair.publicKey)));
}

async function proxyJson(method, url, body = null) {
  const res = await sendWithPromise(
    'anarAccountProxyRequest',
    method,
    url,
    body ? JSON.stringify(body) : '',
  );
  let parsed = null;
  if (res.body) {
    try {
      parsed = JSON.parse(res.body);
    } catch {
      parsed = null;
    }
  }
  if (!res.ok) {
    const detail =
      parsed && typeof parsed.detail === 'string'
        ? parsed.detail
        : `Request failed (${res.status || 'network error'})`;
    const err = new Error(detail);
    err.status = res.status;
    err.body = parsed;
    throw err;
  }
  return parsed;
}

async function openKeyringWithPassword(password) {
  let stored;
  try {
    stored = await proxyJson('GET', `${AUTH_ORIGIN}/api/keyring`);
  } catch (err) {
    if (err.status === 404) {
      return {};
    }
    throw err;
  }

  const env = stored.envelope;
  const salt = base64ToBytes(env.kdfSalt);
  const hash = await globalThis.hashwasm.argon2id({
    password,
    salt,
    parallelism: env.kdfParallelism,
    iterations: env.kdfIterations,
    memorySize: env.kdfMemoryKib,
    hashLength: env.kdfHashLen,
    outputType: 'binary',
  });

  const kek = await crypto.subtle.importKey(
    'raw',
    hash,
    'AES-GCM',
    false,
    ['unwrapKey'],
  );

  const keyringKey = await crypto.subtle.unwrapKey(
    'raw',
    base64ToBytes(env.wrappedMk),
    kek,
    { name: 'AES-GCM', iv: base64ToBytes(env.wrappedMkIv) },
    { name: 'AES-GCM' },
    false,
    ['decrypt'],
  );

  const blob = base64ToBytes(stored.ciphertext);
  const plaintext = new Uint8Array(
    await crypto.subtle.decrypt(
      {
        name: 'AES-GCM',
        iv: blob.slice(0, IV_BYTES),
        additionalData: KEYRING_AAD,
      },
      keyringKey,
      blob.slice(IV_BYTES),
    ),
  );

  const view = new DataView(
    plaintext.buffer,
    plaintext.byteOffset,
    plaintext.byteLength,
  );
  const length = view.getUint32(0);
  const unpadded = plaintext.slice(4, 4 + length);
  const content = JSON.parse(new TextDecoder().decode(unpadded));
  plaintext.fill(0);

  const extracted = {};
  if (content && content.keys && typeof content.keys === 'object') {
    for (const [id, entry] of Object.entries(content.keys)) {
      if (entry && typeof entry.k === 'string') {
        extracted[id] = entry.k;
      }
    }
  }
  return extracted;
}

function showError(msg) {
  const box = el('error-box');
  if (!msg) {
    box.textContent = '';
    box.classList.add('hidden');
  } else {
    box.textContent = msg;
    box.classList.remove('hidden');
  }
}

function setStatus(msg) {
  el('login-status').textContent = msg || '';
}

function renderState(state) {
  document.body.classList.toggle('signed-in', !!(state && state.signedIn));
  showError('');
  el('login-form').classList.add('hidden');
  el('otp-form').classList.add('hidden');
  el('signed-in-view').classList.add('hidden');

  if (!state || !state.signedIn) {
    el('page-title').textContent = 'Sign in with Anar Account';
    el('page-subtitle').textContent =
      'Connect your anar.one account to this browser profile';
    el('login-form').classList.remove('hidden');
    return;
  }

  el('page-title').textContent = 'Anar Account';
  el('page-subtitle').textContent = 'Signed in to anar.one';
  el('signed-in-view').classList.remove('hidden');
  if (addingAccount) {
    el('login-form').classList.remove('hidden');
    el('page-title').textContent = 'Add an Anar Account';
    el('page-subtitle').textContent = 'Add another account to this browser profile';
  }
  if (reconnecting) {
    el('login-form').classList.remove('hidden');
    el('page-title').textContent = 'Reconnect Anar Account';
    el('page-subtitle').textContent = 'Sign in once to restore Anar service sessions in this profile';
  }

  const displayName = state.nickname || state.username || 'Anar User';
  el('profile-name').textContent = displayName;
  el('profile-sub').textContent = `@${state.username}`;

  const avatarEl = el('avatar-circle');
  avatarEl.textContent = '';
  if (state.avatarUrl) {
    const img = document.createElement('img');
    img.src = state.avatarUrl;
    img.alt = displayName;
    img.onerror = () => {
      avatarEl.removeChild(img);
      avatarEl.textContent = displayName.slice(0, 1).toUpperCase();
    };
    avatarEl.appendChild(img);
  } else {
    avatarEl.textContent = displayName.slice(0, 1).toUpperCase();
  }

  const unlockedSet = new Set(state.unlockedServices || []);
  el('security-text').textContent =
    `${unlockedSet.size} service key${unlockedSet.size === 1 ? '' : 's'} encrypted via Windows DPAPI`;

  const grid = el('services-grid');
  grid.textContent = '';
  for (const svc of ANAR_SERVICES) {
    const link = document.createElement('a');
    link.className = 'service-link';
    link.href = svc.url;
    link.target = '_blank';
    link.rel = 'noopener noreferrer';

    const label = document.createElement('span');
    label.textContent = svc.name;
    link.appendChild(label);

    if (unlockedSet.has(svc.id)) {
      const badge = document.createElement('span');
      badge.className = 'key-badge';
      badge.textContent = 'Key';
      link.appendChild(badge);
    }
    grid.appendChild(link);
  }

  const accountList = el('account-list');
  accountList.textContent = '';
  for (const account of state.accounts || []) {
    const row = document.createElement('div');
    row.className = 'account-item';
    const name = document.createElement('span');
    name.textContent = `${account.nickname || account.username}${account.isPrimary ? ' · Primary' : ''}`;
    row.appendChild(name);
    if (!account.isPrimary) {
      const primaryButton = document.createElement('button');
      primaryButton.className = 'secondary';
      primaryButton.textContent = 'Make primary';
      primaryButton.addEventListener('click', async () => {
        renderState(await sendWithPromise('anarAccountSetPrimary', account.userId));
      });
      row.appendChild(primaryButton);
    }
    const removeButton = document.createElement('button');
    removeButton.className = 'secondary';
    removeButton.textContent = 'Remove';
    removeButton.addEventListener('click', async () => {
      renderState(await sendWithPromise('anarAccountRemove', account.userId));
    });
    row.appendChild(removeButton);
    accountList.appendChild(row);
  }
}

async function finishSessionAndStoreKeys(user, password) {
  setStatus('Unlocking Anar service keys...');
  let keys = {};
  try {
    keys = await openKeyringWithPassword(password);
  } catch (err) {
    console.warn('Keyring unwrap skipped:', err);
  }

  let nickname = '';
  let avatarUrl = '';
  try {
    const handoff = await proxyJson(
      'POST', `${AUTH_ORIGIN}/api/handoff/complete`,
      { subsystemId: 'account', requesterPubkeyB64: await requesterPublicKey() },
    );
    await proxyJson(
      'POST', `${ACCOUNT_ORIGIN}/api/auth/session-from-grant`,
      { grant: handoff.sessionGrant, remember: true },
    );
    const profile = await proxyJson('GET', `${ACCOUNT_ORIGIN}/api/profile/me`);
    user.id = profile.id || user.id;
    user.username = profile.username || user.username;
    nickname = profile.nickname || '';
    avatarUrl = profile.avatarUrl || '';
  } catch (err) {
    console.warn('Could not load Anar account profile:', err);
  }


  const state = await sendWithPromise(
    'anarAccountSave',
    user.id || '',
    user.username || '',
    nickname,
    avatarUrl,
    JSON.stringify(keys),
  );
  addingAccount = false;
  reconnecting = false;
  setStatus('');
  renderState(state);
}

async function handleLoginSubmit(event) {
  event.preventDefault();
  showError('');
  let failureStage = 'Loading the secure sign-in module';
  const username = el('username').value.trim();
  const password = el('password').value;
  if (!username || !password) {
    return;
  }

  const submitBtn = el('login-submit');
  submitBtn.disabled = true;
  try {
    setStatus('Authenticating with auth.anar.one (OPAQUE)...');
    await ensureOpaque();
    failureStage = 'Contacting Anar Auth';
    const handle = login_start(password);
    const requestB64 = bytesToBase64(handle.request);

    const startRes = await proxyJson(
      'POST',
      `${AUTH_ORIGIN}/api/auth/opaque/login/start`,
      { username, request: requestB64 },
    );

    let finalizationB64;
    try {
      finalizationB64 = bytesToBase64(
        handle.finish(password, base64ToBytes(startRes.response)),
      );
    } catch {
      throw new Error('Incorrect username or password.');
    }

    const finishRes = await proxyJson(
      'POST',
      `${AUTH_ORIGIN}/api/auth/opaque/login/finish`,
      {
        username,
        loginId: startRes.loginId,
        finalization: finalizationB64,
        remember: true,
        device: true,
      },
    );

    if (finishRes.otpRequired) {
      pendingOtp = {
        pendingToken: finishRes.pendingToken,
        password,
      };
      el('password').value = '';
      el('login-form').classList.add('hidden');
      el('otp-form').classList.remove('hidden');
      setStatus('');
      return;
    }

    el('password').value = '';
    await finishSessionAndStoreKeys(finishRes, password);
  } catch (err) {
    setStatus('');
    showError(`${failureStage}: ${err.message || 'Sign in failed.'}`);
  } finally {
    submitBtn.disabled = false;
  }
}

async function handleOtpSubmit(event) {
  event.preventDefault();
  if (!pendingOtp) {
    return;
  }
  showError('');
  const code = el('otp-code').value.trim();
  const btn = el('otp-submit');
  btn.disabled = true;
  try {
    const user = await proxyJson(
      'POST',
      `${AUTH_ORIGIN}/api/auth/otp/challenge/verify`,
      { pendingToken: pendingOtp.pendingToken, code },
    );
    const pw = pendingOtp.password;
    pendingOtp = null;
    el('otp-code').value = '';
    await finishSessionAndStoreKeys(user, pw);
  } catch (err) {
    showError(err.message || 'Invalid verification code.');
  } finally {
    btn.disabled = false;
  }
}

async function handleSignOut() {
  const btn = el('signout-btn');
  btn.disabled = true;
  try {
    try {
      await proxyJson('POST', `${AUTH_ORIGIN}/api/auth/signout`, {
        everywhere: false,
      });
    } catch {
      // Always clear local credentials even if offline.
    }
    const state = await sendWithPromise('anarAccountSignOut');
    renderState(state);
  } finally {
    btn.disabled = false;
  }
}

async function init() {
  el('login-form').addEventListener('submit', handleLoginSubmit);
  el('otp-form').addEventListener('submit', handleOtpSubmit);
  el('otp-cancel').addEventListener('click', () => {
    pendingOtp = null;
    el('otp-code').value = '';
    renderState({ signedIn: false });
  });
  el('signout-btn').addEventListener('click', handleSignOut);
  el('add-account-btn').addEventListener('click', async () => {
    addingAccount = true;
    renderState(await sendWithPromise('anarAccountGetState'));
  });
  el('reconnect-btn').addEventListener('click', async () => {
    reconnecting = true;
    const state = await sendWithPromise('anarAccountGetState');
    renderState(state);
    el('username').value = state.username || '';
    el('password').focus();
  });

  const state = await sendWithPromise('anarAccountGetState');
  renderState(state);
  if (state && state.signedIn) {
    try {
      await proxyJson('GET', `${AUTH_ORIGIN}/api/handoff/status?subsystem=account`);
      el('reconnect-btn').classList.add('hidden');
    } catch {
      el('reconnect-btn').classList.remove('hidden');
    }
  }
}

init();
