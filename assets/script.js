/**
 * @brief Table of ids that correspond to the unique id of this event
 */
const EVENT_IDS = {
  "on_open_default_directory": "0",
  "on_get_widget_filenames": "1",
  "on_open_widget_by_filename": "2",
  "on_toggle_setting": "3",
  "on_theme_changed": "4",
  "on_agent_user_prompt_received": "5",
  "on_agent_unload_from_memory": "6",
};

/**
 * Default delay of the tooltip before it appears.
 */
const TOOLTIPDELAY = 500;

/**
 * Tooltip timeout trackers
 */
let tooltipTimeouts = new Map();
let loadingInterval = null;
let isModelLoaded = false;

/**
 * Opens the specified scene in the application.
 * @param {string} scene - The name of the scene to open.
 * @param {string} mode - Display style of the scene.
 */
function openScene(scene, mode) {
  hideScenes();
  const element = document.querySelector(`.${scene}`);
  element.style.display = mode;
  updatePageTitle(scene);
}

/**
 * Updates the page title based on the current scene
 * @param {string} scene - The name of the scene
 */
function updatePageTitle(scene) {
  const titleElement = document.getElementById('page-title');
  const titles = {
    'widgets': 'Installed Widgets',
    'development': 'Development',
    'settings': 'Settings'
  };
  titleElement.textContent = titles[scene] || 'Installed Widgets';
}

/**
 * Toggles between light and dark mode
 */
function toggleTheme() {
  document.body.classList.toggle('dark-mode');
  const isDark = document.body.classList.contains('dark-mode');
  const theme = isDark ? 'dark' : 'light';
  localStorage.setItem('theme', theme);
  postMessage('on_theme_changed', theme);
}

/**
 * Initializes theme from localStorage
 */
function initializeTheme() {
  const savedTheme = localStorage.getItem('theme');
  if (savedTheme === 'dark') {
    document.body.classList.add('dark-mode');
  }
  postMessage('on_theme_changed', savedTheme === 'dark' ? 'dark' : 'light');
}

/**
 * Shows tooltip after 2 seconds of hovering
 * @param {HTMLElement} element - Element to show tooltip for
 */
function showTooltipDelayed(element) {
  const timeoutId = setTimeout(() => {
    element.classList.add('show-tooltip');
  }, TOOLTIPDELAY);
  tooltipTimeouts.set(element, timeoutId);
}

/**
 * Hides tooltip and clears timeout
 * @param {HTMLElement} element - Element to hide tooltip for
 */
function hideTooltip(element) {
  const timeoutId = tooltipTimeouts.get(element);
  if (timeoutId) {
    clearTimeout(timeoutId);
    tooltipTimeouts.delete(element);
  }
  element.classList.remove('show-tooltip');
}

/**
 * Initializes tooltip behavior for navigation items
 */
function initializeTooltips() {
  const interactables = document.querySelectorAll('.nav-item, .theme-toggle, .interact');
  interactables.forEach(item => {
    item.addEventListener('mouseenter', () => showTooltipDelayed(item));
    item.addEventListener('mouseleave', () => hideTooltip(item));
  });
}

/**
 * Toggles all settings passed via object
 * @param {object} - settings to toggle
 */
function toggleSettings(settings) {
  for (const [key, value] of Object.entries(settings)) {
    const setting = document.getElementById(key);
    if (typeof value === 'boolean' && value === true) {
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
  newWrapper.setAttribute('data-widget-title', title.toLowerCase());
  newElement.setAttribute('src', path);
  newWrapper.appendChild(newLabel);
  newWrapper.appendChild(newElement);
  parent.appendChild(newWrapper)
}

/**
 * Updates the unload button's enabled state based on isModelLoaded.
 */
function updateUnloadButtonUI() {
  const unloadButton = document.getElementById('unload-button');
  if (unloadButton) {
    unloadButton.disabled = !isModelLoaded;
  }
}

function submitChat(agentMessage) {
  const message = document.getElementById('chat-messages');
  const input = document.getElementById('chat-input');
  const text = (agentMessage ?? input.value).trim();

  if (!text) {
    return;
  }

  const msgDiv = document.createElement('div');
  msgDiv.textContent = text;

  msgDiv.className = 'chat-message';
  msgDiv.style.cssText = `
      padding: 12px 16px;
      background-color: var(--bg-tertiary);
      border-radius: 8px;
      word-wrap: break-word;
      width: fit-content;
      max-width: 75%;
    `;

  if (message.querySelectorAll('.chat-message').length % 2 === 0) {
    msgDiv.style.alignSelf = 'start';
    msgDiv.style.textAlign = 'left';
    msgDiv.setAttribute('assistant', 'true');
    const submitButton = document.getElementById('chat-submit');
    if (submitButton) {
      submitButton.disabled = false;
      submitButton.classList.remove('loading');
      clearInterval(loadingInterval);
      submitButton.textContent = 'Submit';
    }
    isModelLoaded = true;
    updateUnloadButtonUI();
  } else {
    msgDiv.style.alignSelf = 'end';
    msgDiv.style.textAlign = 'right';
    msgDiv.setAttribute('user', 'true');
    onAgentSendUserPrompt(input.value);

    isModelLoaded = true;
    updateUnloadButtonUI();

    const submitButton = document.getElementById('chat-submit');
    if (submitButton) {
      submitButton.disabled = true;
      submitButton.classList.add('loading');
      let dotCount = 0;
      loadingInterval = setInterval(() => {
        dotCount = (dotCount + 1) % 4;
        submitButton.textContent = '.'.repeat(dotCount);
      }, 500);
    }
  }

  message.appendChild(msgDiv);

  input.value = '';
  message.scrollTop = message.scrollHeight;
}

/**
 * Sends a prompt to the C backend and processes the request
 * @param {string} prompt - The user prompt to process
 */
function onAgentSendUserPrompt(prompt) {
  postMessage("on_agent_user_prompt_received", prompt);
}

/**
 * Asks the C backend to unload the current model from memory
 */
function unloadModel() {
  postMessage("on_agent_unload_from_memory");
  isModelLoaded = false;
  updateUnloadButtonUI();
}

/**
 * Adds a special message to the left side of the screen to display embedded HTML.
 * @param {string} htmlContent - The raw HTML content to display.
 */
function addHtmlMessage(htmlContent) {
  const chatMessages = document.getElementById('chat-messages');
  const message = document.createElement('div');
  message.className = 'html-message-in-chat';

  const iframe = document.createElement('iframe');
  iframe.setAttribute('srcdoc', htmlContent);
  iframe.style.width = '500px';
  iframe.style.height = '400px';
  iframe.style.border = 'none';

  message.appendChild(iframe);
  chatMessages.appendChild(message);
}

/**
 * Opens a widget given a file system path.
 * @param {string} path - The file system path to the widget.
 */
function openWidget(path) {
  postMessage("on_open_widget_by_filename", `file://${path}`);
}

/**
 * Adds a list of widgets to the widget UI. This function is called from the C code.
 * @param {object[]} widgets - Array of { title: string, path: string }
 */
function addWidgets(widgets) {
  const el = document.querySelector('.list');
  el.innerHTML = '';

  for (const widget of widgets) {
    addWidget(widget.title, widget.path);
  }
}

/**
 * Filters widgets based on search query
 * @param {string} query - Search query to filter widgets
 */
function filterWidgets(query) {
  const widgets = document.querySelectorAll('.widget');
  const searchQuery = query.toLowerCase().trim();

  widgets.forEach(widget => {
    const widgetTitle = widget.getAttribute('data-widget-title');
    if (widgetTitle.includes(searchQuery)) {
      widget.classList.remove('hidden');
    } else {
      widget.classList.add('hidden');
    }
  });
}

/**
 * Clears the search input and shows all widgets
 */
function clearSearch() {
  const searchInput = document.getElementById('widgets');
  searchInput.value = '';
  filterWidgets('');
  updateClearButtonVisibility();
}

/**
 * Updates the visibility of the clear button based on input value
 */
function updateClearButtonVisibility() {
  const searchInput = document.getElementById('widgets');
  const clearButton = document.querySelector('.clear-search');

  if (searchInput.value.length > 0) {
    clearButton.style.display = 'block';
  } else {
    clearButton.style.display = 'none';
  }
}

/**
 * Initializes search functionality
 */
function initializeSearch() {
  const searchInput = document.getElementById('widgets');
  searchInput.addEventListener('input', (e) => {
    filterWidgets(e.target.value);
    updateClearButtonVisibility();
  });
}

/**
 * Sends a message back to the C code to open the default directory of the application
 */
function openDefaultDirectory() {
  postMessage("on_open_default_directory");
}

/**
 Triggers a postMessages for all platform available
 * @param {string} messageName - Identifier of the message to be posted
 */
function postMessage(messageName, args) {
  const programArgs = {
    eventId: EVENT_IDS[messageName],
    args: args ?? null
  };
  window.chrome?.webview?.postMessage(JSON.stringify(programArgs));       // Windows
  window.webkit?.messageHandlers?.[messageName]?.postMessage(args ?? null); // Linux
}

/**
 * Copies text from a blockquote element to clipboard
 * @param {HTMLElement} button - The copy button that was clicked
 */
function copyBlockquoteText(button) {
  const blockquote = button.closest('blockquote');
  const text = blockquote.querySelector('p').textContent;
  navigator.clipboard.writeText(text).then(() => {
    const originalText = button.textContent;
    button.textContent = 'Copied!';
    setTimeout(() => {
      button.textContent = originalText;
    }, 2000);
  });
}

/**
 * Adds copy buttons to all blockquotes
 */
function initializeCopyButtons() {
  const blockquotes = document.querySelectorAll('.development blockquote');
  blockquotes.forEach(blockquote => {
    const copyButton = document.createElement('button');
    copyButton.className = 'copy-button';
    copyButton.textContent = 'Copy';
    copyButton.onclick = function() { copyBlockquoteText(this); };
    blockquote.style.position = 'relative';
    blockquote.appendChild(copyButton);
  });
}

/**
 * Block right-click context menu
 */
function blockContextMenu() {
  document.addEventListener('contextmenu', (e) => {
    e.preventDefault();
  });
}

/**
 * Block text selection except for blockquote elements
 */
function blockTextSelection() {
  document.addEventListener('selectstart', (e) => {
    if (!e.target.closest('blockquote')) {
      e.preventDefault();
    }
  });
}

/**
 * Executes code after the DOM content is fully loaded and parsed.
 */
document.addEventListener('DOMContentLoaded', async function() {
  initializeTheme();
  initializeTooltips();
  initializeCopyButtons();
  initializeSearch();
  blockContextMenu();
  blockTextSelection();
  updateUnloadButtonUI();
  postMessage("on_get_widget_filenames");
  const chatInput = document.getElementById('chat-input');
  if (chatInput) {
    chatInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        const submitButton = document.getElementById('chat-submit');
        if (submitButton && !submitButton.disabled) {
          e.preventDefault();
          submitChat();
        }
      }
    });
  }
});
