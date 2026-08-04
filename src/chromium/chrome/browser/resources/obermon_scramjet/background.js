const ENGINE_ORIGIN = "http://127.0.0.1:4141";
const ENABLED_KEY = "enabled";
const redirecting = new Set();

function isInternal(url) {
  try {
    const parsed = new URL(url);
    return parsed.origin === ENGINE_ORIGIN;
  } catch {
    return false;
  }
}

function eligible(url) {
  try {
    const parsed = new URL(url);
    return (parsed.protocol === "http:" || parsed.protocol === "https:") &&
      !isInternal(url);
  } catch {
    return false;
  }
}

function internalURL(destination) {
  return `${ENGINE_ORIGIN}/?goto=${encodeURIComponent(destination)}&obermon=1`;
}

function destinationFromInternal(url) {
  if (!isInternal(url)) return null;
  try {
    const value = new URL(url).searchParams.get("goto");
    return value && eligible(value) ? value : null;
  } catch {
    return null;
  }
}

async function enabled() {
  const state = await chrome.storage.local.get(ENABLED_KEY);
  return state[ENABLED_KEY] !== false;
}

async function mediateTab(tabId, url) {
  if (!eligible(url) || redirecting.has(tabId)) return false;
  redirecting.add(tabId);
  try {
    await chrome.tabs.update(tabId, {url: internalURL(url)});
    return true;
  } finally {
    setTimeout(() => redirecting.delete(tabId), 500);
  }
}

chrome.runtime.onInstalled.addListener(async () => {
  const state = await chrome.storage.local.get(ENABLED_KEY);
  if (state[ENABLED_KEY] === undefined) {
    await chrome.storage.local.set({[ENABLED_KEY]: true});
  }
});

chrome.webNavigation.onBeforeNavigate.addListener(async details => {
  if (details.frameId !== 0 || details.tabId < 0) return;
  if (!(await enabled())) return;
  await mediateTab(details.tabId, details.url);
});

chrome.runtime.onMessage.addListener((message, _sender, respond) => {
  (async () => {
    if (message?.type === "mode.get") {
      return {ok: true, enabled: await enabled()};
    }
    if (message?.type === "mode.set") {
      const next = message.enabled !== false;
      await chrome.storage.local.set({[ENABLED_KEY]: next});
      const [tab] = await chrome.tabs.query({active: true, currentWindow: true});
      if (tab?.id != null && tab.url) {
        if (next) {
          await mediateTab(tab.id, tab.url);
        } else {
          const destination = destinationFromInternal(tab.url);
          if (destination) await chrome.tabs.update(tab.id, {url: destination});
        }
      }
      return {ok: true, enabled: next};
    }
    if (message?.type === "tool.open") {
      const tool = ["requests", "playground", "settings"].includes(message.tool)
        ? message.tool : "requests";
      await chrome.tabs.create({url: `${ENGINE_ORIGIN}/?tool=${tool}`});
      return {ok: true};
    }
    throw new Error("Unknown Scramjet request");
  })().then(respond).catch(error => respond({ok: false, error: error.message}));
  return true;
});
