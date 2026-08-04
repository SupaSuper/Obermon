const ENGINE_ORIGIN = "http://127.0.0.1:4141";
const ENABLED_PREF = "obermon.scramjet_enabled";

function eligible(url) {
  try {
    const parsed = new URL(url);
    return (parsed.protocol === "http:" || parsed.protocol === "https:") &&
      parsed.origin !== ENGINE_ORIGIN;
  } catch {
    return false;
  }
}

function getPref() {
  return new Promise((resolve, reject) => {
    chrome.settingsPrivate.getPref(ENABLED_PREF, pref => {
      const error = chrome.runtime.lastError;
      if (error) reject(new Error(error.message));
      else resolve(pref?.value !== false);
    });
  });
}

function setPref(value) {
  return new Promise((resolve, reject) => {
    chrome.settingsPrivate.setPref(ENABLED_PREF, value, "obermon-scramjet", success => {
      const error = chrome.runtime.lastError;
      if (error) reject(new Error(error.message));
      else if (!success) reject(new Error("Obermon rejected the Scramjet mode change."));
      else resolve(value);
    });
  });
}

async function reloadCurrentTabForMode(enabled) {
  const [tab] = await chrome.tabs.query({active: true, currentWindow: true});
  if (tab?.id == null || !eligible(tab.url || "")) return;
  // The browser process sees the updated native pref before this navigation.
  // Enabled navigations are intercepted by ScramjetNavigationThrottle; disabled
  // navigations proceed directly to the same destination URL.
  await chrome.tabs.update(tab.id, {url: tab.url});
}

chrome.runtime.onMessage.addListener((message, _sender, respond) => {
  (async () => {
    if (message?.type === "mode.get") {
      return {ok: true, enabled: await getPref()};
    }
    if (message?.type === "mode.set") {
      const next = message.enabled !== false;
      await setPref(next);
      await reloadCurrentTabForMode(next);
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
