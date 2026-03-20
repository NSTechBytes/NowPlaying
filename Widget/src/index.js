// ============================================================================
// NowPlaying Widget - Main Process Script
// ============================================================================
// This script serves as the main entry point for the NowPlaying widget.
// It manages communication between the Novadesk addon and the UI window,
// handling media playback controls and status updates.
// ============================================================================

import { widgetWindow, addon } from "novadesk";

// ============================================================================
// Addon Initialization
// ============================================================================

/**
 * Load the NowPlaying addon DLL
 * This addon provides media player integration and control functionality
 */
const nowPlaying_Addon = addon.load(
  "addon\\NowPlaying.dll"
);

// ============================================================================
// Media Status Management
// ============================================================================

/**
 * Retrieves current media statistics from the addon and sends them to the UI
 * This function is called periodically to keep the UI updated with latest info
 */
function pushNowPlaying() {
  ipcMain.send("update-nowPlaying", nowPlaying_Addon.stats());
}

// Send initial now playing data immediately
pushNowPlaying();

// Update media information every second (1000ms)
setInterval(() => {
  pushNowPlaying();
}, 1000);

// ============================================================================
// IPC Event Handlers - User Actions
// ============================================================================

/**
 * Handles seek requests from the UI
 * Seeks to a position in the current track based on percentage (0-100)
 * @param {Object} event - IPC event object
 * @param {Object} data - Data containing 'percent' property (0-100)
 */
ipcMain.on("nowPlaying_seek", (event, data) => {
  // Validate and clamp percentage value to valid range [0, 100]
  const pct = data && typeof data.percent === "number" ? data.percent : 0;
  const clamped = Math.max(0, Math.min(100, pct));

  // Seek to position using addon API
  // Second parameter 'true' indicates percentage-based seeking
  nowPlaying_Addon.setPosition(clamped, true);
});

/**
 * Handles initial data requests from UI when it loads
 * Triggers an immediate update of now playing information
 */
ipcMain.on("nowPlaying_request", () => {
  pushNowPlaying();
});

/**
 * Handles playback control actions from the UI
 * Supports: previous track, next track, play/pause toggle
 * @param {Object} event - IPC event object
 * @param {Object} data - Data containing 'action' property
 */
ipcMain.on("nowPlaying_action", (event, data) => {
  const action = data && data.action ? data.action : "";

  // Route action to appropriate addon method
  if (action === "previous") {
    nowPlaying_Addon.previous();
  } else if (action === "next") {
    nowPlaying_Addon.next();
  } else if (action === "playPause") {
    nowPlaying_Addon.playPause();
  }
});

// ============================================================================
// Widget Window Configuration
// ============================================================================

/**
 * Create and configure the NowPlaying widget window
 * This defines the visual container for the media player UI
 */
var nowPlayingWindow = new widgetWindow({
  id: "now-playing",
  width: 300,
  height: 400,
  backgroundColor: "#000000",
  script: "ui/script.ui.js",
});
