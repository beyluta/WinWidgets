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
 * Hide all scenes from the application.
 */
function hideScenes() {
  const mainElements = document.querySelectorAll('main');
  for (const element of mainElements) {
    element.style.display = 'none';
  }
}

/**
 * Toggles the selected setting
 * @param {string} setting - HtmlElement to be modified
 */
function toggleSetting(setting) {
  if (setting.classList.contains('enabled')) {
    setting.classList.remove('enabled');
  } else {
    setting.classList.add('enabled');
  }
}

/**
 * Adds a widget to the local widget UI
 * @param {string} path - Full path of the widget to add 
 */
function addWidget(path) {
  const parent = document.querySelector('.list');
  const newWrapper = document.createElement('div');
  const newElement = document.createElement('iframe');
  newWrapper.setAttribute('onclick', `openWidget('${path}')`);
  newElement.setAttribute('src', path);
  newWrapper.appendChild(newElement);
  parent.appendChild(newWrapper)
}

/**
 * Opens a widget given a file system path.
 * @param {string} path - The file system path to the widget.
 * @returns {Promise<void>} Resolves when the widget is opened.
 */
async function openWidget(path) {
  window.webkit.messageHandlers.on_open_widget_by_filename.postMessage(`file://${path}`);
}

/**
 * Adds a list of widgets to the widget UI. This function is called from the C code.
 * @param {string} path - Full path of the widget to add 
 */
function addWidgets(paths) {
  const parsedInput = parseSingleQuotedArray(paths);
  parsedInput.forEach((path) => {
    addWidget(path);
  });
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
 * Executes code after the DOM content is fully loaded and parsed.
 */
document.addEventListener('DOMContentLoaded', async function() {
  // Native C function to get the path of all HTML files
  window.webkit.messageHandlers.on_get_widget_filenames.postMessage('test');
});
