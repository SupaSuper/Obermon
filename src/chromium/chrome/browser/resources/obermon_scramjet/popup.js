const root = document.querySelector("main");
const toggle = document.querySelector("#toggle");
const error = document.querySelector("#error");

let busy = false;

function setError(message = "") {
  if (error.textContent !== message) error.textContent = message;
}

async function send(message) {
  const response = await chrome.runtime.sendMessage(message);
  if (!response?.ok) {
    throw new Error(response?.error || "Scramjet request failed");
  }
  return response;
}

root.addEventListener("click", async event => {
  const target = event.target instanceof Element
    ? event.target.closest("button[data-tool]")
    : null;
  if (!target || busy) return;

  busy = true;
  setError();
  try {
    await send({type: "tool.open", tool: target.dataset.tool});
    window.close();
  } catch (cause) {
    setError(cause instanceof Error ? cause.message : String(cause));
    busy = false;
  }
});

toggle.addEventListener("change", async () => {
  if (busy) return;
  busy = true;
  toggle.disabled = true;
  setError();

  const requested = toggle.checked;
  try {
    const state = await send({type: "mode.set", enabled: requested});
    if (toggle.checked !== state.enabled) toggle.checked = state.enabled;
  } catch (cause) {
    toggle.checked = !requested;
    setError(cause instanceof Error ? cause.message : String(cause));
  } finally {
    toggle.disabled = false;
    busy = false;
  }
});

async function initialize() {
  try {
    const state = await send({type: "mode.get"});
    if (toggle.checked !== state.enabled) toggle.checked = state.enabled;
  } catch (cause) {
    setError(cause instanceof Error ? cause.message : String(cause));
  }
}

void initialize();
