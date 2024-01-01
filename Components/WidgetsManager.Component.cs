using CefSharp;
using CefSharp.WinForms;
using Hooks;
using Microsoft.Win32;
using Models;
using Modules;
using Newtonsoft.Json;
using Service;
using Services;
using Snippets;
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Timers;
using System.Windows.Forms;
using WidgetsDotNet.Properties;
using WidgetsDotNet.Services;

namespace Components
{
    internal class WidgetsManagerComponent : WidgetManagerModel
    {
        private string _htmlPath = String.Empty;
        private WidgetForm _window;
        private ChromiumWebBrowser _browser;
        private IntPtr _handle;
        private Configuration _configuration;
        private string managerUIPath = AssetService.assetsPath + "/index.html";
        private RegistryKey registryKey = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", true);
        private NotifyIcon notifyIcon;
        private FormService formService = new FormService();
        private HTMLDocService HTMLDocService = new HTMLDocService();
        private TemplateService templateService = new TemplateService();
        private WidgetService widgetService = new WidgetService();
        private TimerService timerService = new TimerService();
        private WidgetManager widgetManager = new WidgetManager();

        public override string htmlPath 
        { 
            get { return _htmlPath; }
            set { _htmlPath = value; }
        }

        public override WidgetForm window
        {
            get { return _window; }
            set { _window = value; }
        }

        public override ChromiumWebBrowser browser
        {
            get { return _browser; }
            set { _browser = value; }
        }

        public override IntPtr handle
        {
            get { return _handle; }
            set { _handle = value; }
        }

        public override Configuration configuration
        {
            get { return _configuration; }
            set { _configuration = value; }
        }

        public WidgetsManagerComponent()
        {
            CefSettings options = new CefSettings();
            options.CefCommandLineArgs.Add("disable-web-security");
            Cef.Initialize(options);

            AssetService.CreateHTMLFilesDirectory();

            this.templateService.MoveTemplatesToWidgetsPath();
            this.configuration = AssetService.GetConfigurationFile();

            notifyIcon = new NotifyIcon();
            notifyIcon.Icon = Resources.favicon;
            notifyIcon.Text = "WinWidgets";
            notifyIcon.Visible = true;
            notifyIcon.ContextMenu = new ContextMenu(new MenuItem[]
            { new MenuItem("Open Manager", OnOpenApplication),
              new MenuItem("Stop All Widgets", OnStopAllWidgets),
              new MenuItem("-"),
              new MenuItem("Quit", delegate { Application.Exit(); })
            });
            notifyIcon.MouseDoubleClick += NotifyIconDoubleClick;

            Rectangle screenResolution = Screen.PrimaryScreen.Bounds;
            int width = screenResolution.Width / 2;
            int height = screenResolution.Height - 200;

            CreateWindow(width, height, "WinWidgets", false);
        }

        public override void CreateWindow(int width, int height, string title, bool save, Point position = default(Point), bool? alwaysOnTop = null)
        {
            window = new WidgetForm(false);
            window.Size = new Size(width, height);
            window.StartPosition = FormStartPosition.CenterScreen;
            window.Text = title;
            window.Activated += delegate { handle = window.Handle; };
            window.Icon = Resources.favicon;
            window.Resize += OnFormResized;
            window.ShowInTaskbar = false;
            AppendWidget(window, managerUIPath);
            window.ShowDialog();
        }

        public override void AppendWidget(Form f, string path)
        {
            browser = new ChromiumWebBrowser(path);
            browser.JavascriptMessageReceived += OnBrowserMessageReceived;
            browser.IsBrowserInitializedChanged += OnBrowserInitialized;
            browser.MenuHandler = new WidgetManagerMenuHandler();
            f.Controls.Add(browser);
        }

        private void ReloadWidgets()
        {
            string template = 
                $"var container = document.getElementById('widgets');"
                + $"container.innerHTML = '';"
                + $"setVersion('{this.configuration.version}');"
                + "document.getElementById('folder').onclick = () => CefSharp.PostMessage('widgetsFolder');"
                + "var switches = document.getElementsByClassName('switch');"
                + "for (let s of switches) {"
                + "const setting = s.getAttribute('setting');"
                + "if (setting == 'startup') {"
                + $"{(registryKey.GetValue("WinWidgets") != null ? "s.classList.add('switchon');" : "")}"
                + "}"
                + "else if (setting == 'widgetStartup') {"
                + $"{(AssetService.GetConfigurationFile().isWidgetAutostartEnabled ? "s.classList.add('switchon');" : "")}"
                + "}" 
                + "else if (setting == 'widgetHideOnFullscreenApplication') {"
                + $"{(AssetService.GetConfigurationFile().isWidgetFullscreenHideEnabled ? "s.classList.add('switchon');" : "")}"
                + "}"
                + "else if (setting == 'managerHideOnStart') {"
                + $"{(AssetService.GetConfigurationFile().hideWidgetManagerOnStartup ? "s.classList.add('switchon');" : "")}"
                + "}}";
            string[] files = AssetService.GetPathToHTMLFiles(AssetService.widgetsPath);

            for (int i = 0; i < files.Length; i++)
            {
                _htmlPath = files[i];
                string localWidgetPath = _htmlPath.Replace('\\', '/');

                template += $@"
                    var e{i} = document.createElement('div');"
                    + $"e{i}.classList.add('widget');"
                    + $"e{i}.classList.add('flex-row');"
                    + $"e{i}.style.width = '{(this.HTMLDocService.GetMetaTagValue("previewSize", _htmlPath) != null ? this.HTMLDocService.GetMetaTagValue("previewSize", _htmlPath).Split(' ')[0] : null)}px';"
                    + $"e{i}.style.minHeight = '{(this.HTMLDocService.GetMetaTagValue("previewSize", _htmlPath) != null ? this.HTMLDocService.GetMetaTagValue("previewSize", _htmlPath).Split(' ')[1] : null)}px';"
                    + $"e{i}.setAttribute('name', '{this.HTMLDocService.GetMetaTagValue("applicationTitle", _htmlPath)}');"
                    + $"e{i}.innerHTML = `<p>{this.HTMLDocService.GetMetaTagValue("applicationTitle", _htmlPath)}</p> <iframe src='file:///{localWidgetPath}'></iframe>`;"
                    + $"var l{i} = document.createElement('span');"
                    + $"e{i}.appendChild(l{i});"
                    + $"l{i}.classList.add('label');"
                    + $"l{i}.innerText = '{this.HTMLDocService.GetMetaTagValue("applicationTitle", _htmlPath)}';"
                    + $"document.getElementById('widgets').appendChild(e{i});"
                    + $"e{i}.onclick = () => CefSharp.PostMessage('{i}');"
                 ;
            }

            browser.ExecuteScriptAsyncWhenPageLoaded(template);
        }

        public override void OpenWidgets()
        {
            foreach (WidgetConfiguration widgetConfiguration in this.configuration.lastSessionWidgets)
            {
                OpenWidget(widgetConfiguration.path, new Point(
                    widgetConfiguration.position.X, 
                    widgetConfiguration.position.Y
                ),
                widgetConfiguration.alwaysOnTop);
            }
        }

        public override void OpenWidget(int id)
        {
            WidgetComponent widget = new WidgetComponent();
            AssetService.widgets.AddWidget(widget);

            widget.htmlPath = AssetService.GetPathToHTMLFiles(AssetService.widgetsPath)[id];
            widget.CreateWindow(300, 300, $"Widget{id}", true);
        }

        public override void OpenWidget(string path, Point position, bool? alwaysOnTop)
        {
            WidgetComponent widget = new WidgetComponent();
            AssetService.widgets.AddWidget(widget);

            widget.htmlPath = path;
            widget.CreateWindow(300, 300, $"Widget{path}", false, position, alwaysOnTop);
        }

        private void OnStopAllWidgets(object sender, EventArgs e)
        {
            this.widgetService.CloseAllWidgets(true);
        }

        private void OnFormResized(object sender, EventArgs e)
        {
            if (this.formService.FormStateAssert(window, FormWindowState.Minimized))
            {
                this.formService.SetWindowOpacity(this.window, 0);
            }
        }

        private void OnOpenApplication(object sender, EventArgs e)
        {
            this.formService.WakeWindow(this.window);
        }

        private void NotifyIconDoubleClick(object sender, MouseEventArgs e)
        {
            this.formService.WakeWindow(this.window);
        }

        private void OnBrowserInitialized(object sender, EventArgs e)
        {
            FileSystemWatcher fileWatcher = new FileSystemWatcher(AssetService.widgetsPath);
            fileWatcher.NotifyFilter = NotifyFilters.Attributes
                                 | NotifyFilters.CreationTime
                                 | NotifyFilters.DirectoryName
                                 | NotifyFilters.FileName
                                 | NotifyFilters.LastAccess
                                 | NotifyFilters.LastWrite
                                 | NotifyFilters.Security
                                 | NotifyFilters.Size;

            fileWatcher.Changed += delegate { ReloadWidgets(); };
            fileWatcher.Created += delegate { ReloadWidgets(); };
            fileWatcher.Deleted += delegate { ReloadWidgets(); };
            fileWatcher.Renamed += delegate { ReloadWidgets(); };

            fileWatcher.Filter = "*.html";
            fileWatcher.IncludeSubdirectories = true;
            fileWatcher.EnableRaisingEvents = true;

            this.timerService.CreateTimer(1000, OnInitializeHooks, false, true);

            this.browser.BeginInvoke(new Action(delegate
            {
                if (this.configuration.isWidgetAutostartEnabled)
                {
                    OpenWidgets();
                }

                if (this.configuration.hideWidgetManagerOnStartup)
                {
                    this.widgetManager.MinimizeWidgetManager(window);
                }
            }));

            ReloadWidgets();
        }

        private void OnInitializeHooks(object sender, ElapsedEventArgs e)
        {
            browser.BeginInvoke(new Action(delegate
            {
                UserActivityHook userActivityHook = new UserActivityHook();
                userActivityHook.KeyDown += new KeyEventHandler(OnKeyDown);
                userActivityHook.KeyUp += new KeyEventHandler(OnKeyUp);
                userActivityHook.OnMouseActivity += new MouseEventHandler(OnMouseActivity);

                HardwareActivityHook hardwareActivityHook = new HardwareActivityHook();
                hardwareActivityHook.OnBattery += OnBatteryChanged;
                hardwareActivityHook.OnSpaceAvailable += OnSpaceAvailableChanged;
                hardwareActivityHook.OnAnyApplicationFullscrenStatusChanged += OnAnyApplicationFullscrenStatusChanged;
            }));
        }

        private void CallJavaScriptFunction(string data, HardwareEvent hardwareEvent)
        {
            for (int i = 0; i < AssetService.widgets.Widgets.Count; i++)
            {
                WidgetComponent widget = (WidgetComponent)AssetService.widgets.Widgets[i];

                switch (hardwareEvent)
                {
                    case HardwareEvent.NativeKeys:
                        if (JsonConvert.DeserializeObject<PeripheralAction>(data).buttonPressed == "Left")
                        {
                            widget.moveModeEnabled = false;
                        }

                        this.widgetService.InjectJavascript(widget, "if (typeof onNativeKeyEvents === 'function') { onNativeKeyEvents(" + data + "); }");
                        break;

                    case HardwareEvent.Battery:
                        this.widgetService.InjectJavascript(widget, "if (typeof onNativeBatteryEvent === 'function') { onNativeBatteryEvent(" + data + "); }");
                        break;

                    case HardwareEvent.SpaceAvailable:
                        this.widgetService.InjectJavascript(widget, "if (typeof onNativeSpaceAvailableEvent === 'function') { onNativeSpaceAvailableEvent(" + data + "); }");
                        break;
                }
            }
        }

        private void OnKeyDown(object sender, KeyEventArgs e)
        {
            PeripheralAction action = new PeripheralAction()
            {
                keycode = e.KeyValue.ToString(),
                state = "keyDown",
                eventType = "keyboardEvent",
            };

            CallJavaScriptFunction(JsonConvert.SerializeObject(action), HardwareEvent.NativeKeys);
        }

        private void OnKeyUp(object sender, KeyEventArgs e)
        {
            PeripheralAction action = new PeripheralAction()
            {
                keycode = e.KeyValue.ToString(),
                state = "keyUp",
                eventType = "keyboardEvent",
            };

            CallJavaScriptFunction(JsonConvert.SerializeObject(action), HardwareEvent.NativeKeys);
        }

        private void OnMouseActivity(object sender, MouseEventArgs e)
        {
            PeripheralAction action = new PeripheralAction()
            {
                xPosition = e.X.ToString(),
                yPosition = e.Y.ToString(),
                buttonPressed = e.Button.ToString(),
                buttonPressCount = e.Clicks.ToString(),
                mouseWheelOffset = e.Delta.ToString(),
                eventType = "mouseEvent",
            };

            CallJavaScriptFunction(JsonConvert.SerializeObject(action), HardwareEvent.NativeKeys);
        }

        private void OnBatteryChanged(string batteryInfo)
        {
            CallJavaScriptFunction(batteryInfo, HardwareEvent.Battery);
        }

        private void OnSpaceAvailableChanged(long freeSpace)
        {
            CallJavaScriptFunction(freeSpace.ToString(), HardwareEvent.SpaceAvailable);
        }

        private void OnAnyApplicationFullscrenStatusChanged(bool fullscreen)
        {
            this.configuration = AssetService.GetConfigurationFile();

            if (this.configuration.isWidgetFullscreenHideEnabled)
            {
                if (fullscreen)
                {
                    this.widgetService.CloseAllWidgets(false);
                }
                else
                {
                    this.OpenWidgets();
                }
            }
        }

        private void OnBrowserMessageReceived(object sender, JavascriptMessageReceivedEventArgs e)
        {
            switch (e.Message)
            {
                case "widgetsFolder":
                    Process.Start(AssetService.widgetsPath);
                    break;

                case "startup":
                    if (registryKey.GetValue("WinWidgets") == null)
                    {
                        registryKey.SetValue("WinWidgets", Application.ExecutablePath);
                    }
                    else
                    {
                        registryKey.DeleteValue("WinWidgets");
                    }
                    break;

                case "widgetStartup":
                    {
                        Configuration configuration = AssetService.GetConfigurationFile();
                        configuration.isWidgetAutostartEnabled = !configuration.isWidgetAutostartEnabled;
                        AssetService.OverwriteConfigurationFile(configuration);
                    }
                    break;

                case "widgetHideOnFullscreenApplication":
                    {
                        Configuration configuration = AssetService.GetConfigurationFile();
                        configuration.isWidgetFullscreenHideEnabled = !configuration.isWidgetFullscreenHideEnabled;
                        AssetService.OverwriteConfigurationFile(configuration);
                    }
                    break;

                case "managerHideOnStart":
                    {
                        Configuration configuration = AssetService.GetConfigurationFile();
                        configuration.hideWidgetManagerOnStartup = !configuration.hideWidgetManagerOnStartup;
                        AssetService.OverwriteConfigurationFile(configuration);
                    }
                    break;

                default:
                    OpenWidget(int.Parse((string)e.Message));
                    break;
            }
        }
    }
}