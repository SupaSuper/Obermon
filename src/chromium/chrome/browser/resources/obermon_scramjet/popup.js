const toggle = document.querySelector("#toggle");
const error = document.querySelector("#error");

async function send(message) {
  const response = await chrome.runtime.sendMessage(message);
  if (!response?.ok) throw new Error(response?.error || "Scramjet request failed");
  return response;
}

document.querySelectorAll("[data-tool]").forEach(button => {
  button.addEventListener("click", async () => {
    try {
      error.textContent = "";
      await send({type: "tool.open", tool: button.dataset.tool});
      window.close();
    } catch (cause) {
      error.textContent = cause.message;
    }
  });
});

toggle.addEventListener("change", async () => {
  try {
    error.textContent = "";
    const state = await send({type: "mode.set", enabled: toggle.checked});
    toggle.checked = state.enabled;
  } catch (cause) {
    toggle.checked = !toggle.checked;
    error.textContent = cause.message;
  }
});

send({type: "mode.get"}).then(state => toggle.checked = state.enabled)
  .catch(cause => error.textContent = cause.message);
