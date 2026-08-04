const ENGINE_ORIGIN = "http://127.0.0.1:4141";
const ENABLED_PREF = "obermon.scramjet_enabled";
const VALID_TOOLS = new Set(["requests", "playground", "settings"]);

let enabledCache;
let pendingPrefRead = null;
let pendingPrefWrite = null;

function eligible(url) {
  return typeof url === "string" &&
    (url.startsWith("http://") || url.startsWith("https://")) &&
    !url.startsWith(ENGINE_ORIGIN);
}

function readPref() {
  if (enabledCache !== undefined) return Promise.resolve(enabledCache);
  if (pendingPrefRead) return pendingPrefRead;

  pendingPrefRead = new Promise((resolve, reject) => {
    chrome.settingsPrivate.getPref(ENABLED_PREF, pref => {
      const error = chrome.runtime.lastError;
      if (error) {
        reject(new Error(error.message));
        return;
      }
      enabledCache = pref?.value !== false;
      resolve(enabledCache);
    });
  }).finally(() => {
    pendingPrefRead = null;
  });

  return pendingPrefRead;
}

function writePref(value) {
  if (enabledCache === value && !pendingPrefWrite) {
    return Promise.resolve(false);
  }
  if (pendingPrefWrite) return pendingPrefWrite;

  pendingPrefWrite = new Promise((resolve, reject) => {
    chrome.settingsPrivate.setPref(
      ENABLED_PREF,
      value,
      "obermon-scramjet",
      success => {
        const error = chrome.runtime.lastError;
        if (error) {
          reject(new Error(error.message));
          return;
        }
        if (!success) {
          reject(new Error("Obermon rejected the Scramjet mode change."));
          return;
        }
        const changed = enabledCache !== value;
        enabledCache = value;
        resolve(changed);
      },
    );
  }).finally(() => {
    pendingPrefWrite = null;
  });

  return pendingPrefWrite;
}

chrome.settingsPrivate.onPrefsChanged.addListener(prefs => {
  for (const pref of prefs) {
    if (pref.key === ENABLED_PREF) {
      enabledCache = pref.value !== false;
      break;
    }
  }
});

async function reloadCurrentTab() {
  const tabs = await chrome.tabs.query({active: true, currentWindow: true});
  const tab = tabs[0];
  if (tab?.id == null || !eligible(tab.url)) return;
  await chrome.tabs.update(tab.id, {url: tab.url});
}

const handlers = {
  async "mode.get"() {
    return {ok: true, enabled: await readPref()};
  },

  async "mode.set"(message) {
    const next = message.enabled !== false;
    const changed = await writePref(next);
    if (changed) await reloadCurrentTab();
    return {ok: true, enabled: next};
  },

  async "tool.open"(message) {
    const tool = VALID_TOOLS.has(message.tool) ? message.tool : "requests";
    await chrome.tabs.create({url: `${ENGINE_ORIGIN}/?tool=${tool}`});
    return {ok: true};
  },
};

chrome.runtime.onMessage.addListener((message, _sender, respond) => {
  const handler = handlers[message?.type];
  if (!handler) {
    respond({ok: false, error: "Unknown Scramjet request"});
    return false;
  }

  Promise.resolve(handler(message))
    .then(respond)
    .catch(error => respond({
      ok: false,
      error: error instanceof Error ? error.message : String(error),
    }));
  return true;
});
