async function sendCommand(endpoint) {
  const statusEl = document.getElementById("status");
  try {
    const response = await fetch(endpoint);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const text = await response.text();
    statusEl.textContent = text;
  } catch (err) {
    statusEl.textContent = `Error: ${err.message}`;
  }
}

// Modal and Alarm Elements
const abortModal = document.getElementById("abortModal");
const confirmAbortBtn = document.getElementById("confirmAbort");
const cancelAbortBtn = document.getElementById("cancelAbort");

document.getElementById("abortBtn").addEventListener("click", () => {
  abortModal.style.display = "flex";
  abortModal.classList.add("active");
});

confirmAbortBtn.addEventListener("click", () => {
  abortModal.style.display = "none";
  abortModal.classList.remove("active");
  sendCommand("/api/abort");
});

cancelAbortBtn.addEventListener("click", () => {
  abortModal.style.display = "none";
  abortModal.classList.remove("active");
});

document.getElementById("sasBtn").addEventListener("click", () => {
  sendCommand("/api/sas");
});

document.getElementById("lightsBtn").addEventListener("click", () => {
  sendCommand("/api/lights");
});

document.getElementById("stageBtn").addEventListener("click", () => {
  sendCommand("/api/stage");
});

document.getElementById("rcsBtn").addEventListener("click", () => {
  sendCommand("/api/rcs");
});

document.getElementById("gearsBtn").addEventListener("click", () => {
  sendCommand("/api/gears");
});

document.getElementById("brakesBtn").addEventListener("click", () => {
  sendCommand("/api/brakes");
});

//Speed and altitude polling
async function updateStatus() {
  const statusEl = document.getElementById("status");
  try {
    const [velocityRes, altitudeRes] = await Promise.all([
      fetch(`api/surfaceVelocity`),
      fetch("api/verticalAltitude")
    ]);

    if (!velocityRes.ok || !altitudeRes.ok) {
      throw new Error("Failed to fetch telemetry data");
    }

    const surfaceVelocity = await velocityRes.text();
    const verticalAltitude = await altitudeRes.text();

    statusEl.innerHTML = `
      <b>Surface Velocity:</b> ${surfaceVelocity} m/s<br>
      <b>Vertical Altitude:</b> ${verticalAltitude} m
    `;
  } catch (err) {
    statusEl.textContent = `Telemetry Error: ${err.message}`;
  }
}

// Update status once per second
setInterval(updateStatus, 1000);

