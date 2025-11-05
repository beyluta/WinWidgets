/**
 * @brief Table of ids that correspond to the unique id of this event
 */
const EVENT_IDS = {
  "on_open_default_directory": "0",
  "on_get_widget_filenames": "1",
  "on_open_widget_by_filename": "2",
  "on_toggle_setting": "3"
};

/**
 * Opens the specified scene in the application.
 * @param {string} scene - The name of the scene to open.
 * @param {string} mode - Display style of the scene.
 */
function openScene(scene, mode) {
  hideScenes();
  const element = document.querySelector(`.${scene}`);
  element.style.display = mode;
}

/**
 * Toggles all settings passed via object
 * @param {object} - settings to toggle
 */
function toggleSettings(settings) {
  for (const [key, value] of Object.entries(settings)) {
    const setting = document.getElementById(key);
    if (typeof value === 'boolean' && value === true)
    {
      toggleSetting(setting);
    }
  }
}

/**
 * Toggles the selected setting
 * @param {string} setting - HtmlElement to be modified
 * @param {boolean} notifyParent - Whether C should be notified of this change
 */
function toggleSetting(setting, notifyParent = false) {
  const id = setting.getAttribute('id');

  const isEnabled = setting.classList.contains('enabled');
  if (isEnabled) {
    setting.classList.remove('enabled');
  } else {
    setting.classList.add('enabled');
  }

  if (notifyParent) {
    postMessage('on_toggle_setting', id);
  }
}

/**
 * Hide all scenes from the application.
 */
function hideScenes() {
  const mainElements = document.querySelectorAll('main');
  for (const element of mainElements) {
    element.style.display = 'none';
  }
}

/**
 * Adds a widget to the local widget UI
 * @param {string} title - Name of the widget
 * @param {string} path - Full path of the widget to add
 */
function addWidget(title, path) {
  const parent = document.querySelector('.list');
  const newWrapper = document.createElement('div');
  const newElement = document.createElement('iframe');
  const newLabel = document.createElement('p');
  newLabel.innerText = title;
  newLabel.setAttribute('class', 'label');
  newWrapper.setAttribute('onclick', `openWidget('${path}')`);
  newWrapper.setAttribute('class', 'widget');
  newElement.setAttribute('src', path);
  newWrapper.appendChild(newLabel);
  newWrapper.appendChild(newElement);
  parent.appendChild(newWrapper)
}

/**
 * Opens a widget given a file system path.
 * @param {string} path - The file system path to the widget.
 * @returns {Promise<void>} Resolves when the widget is opened.
 */
async function openWidget(path) {
  postMessage("on_open_widget_by_filename", `file://${path}`);
}

/**
 * Adds a list of widgets to the widget UI. This function is called from the C code.
 * @param {object[]} widgets - Array of { title: string, path: string }
 */
function addWidgets(widgets) {
  for (const widget of widgets) {
    addWidget(widget.title, widget.path);
  }
}

/**
 * Converts a string representation of an array using single quotes
 * (e.g., "['a', 'b', 'c']") to a real JavaScript array.
 * @param {string} str - The string array with single quotes.
 * @returns {Array} The parsed JavaScript array.
 */
function parseSingleQuotedArray(str) {
  // FIXME: C must send double quoted arrays
  return JSON.parse(str.replace(/'/g, '"'));
}

/**
 * Sends a message back to the C code to open the default directory of the application
 */
function openDefaultDirectory() {
  postMessage("on_open_default_directory");
}

/**
 * Triggers a postMessages for all platform available
 * @param {string} messageName - Identifier of the message to be posted
 */
function postMessage(messageName, args) {
  const programArgs = {
    eventId: EVENT_IDS[messageName],
    args: args ?? null
  };
  window.chrome.webview.postMessage(JSON.stringify(programArgs));       // Windows
  window.webkit.messageHandlers[messageName].postMessage(args ?? null); // Linux
}

/**
 * Executes code after the DOM content is fully loaded and parsed.
 */
document.addEventListener('DOMContentLoaded', async function() {
  postMessage("on_get_widget_filenames");
});
