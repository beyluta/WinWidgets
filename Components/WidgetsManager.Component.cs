using CefSharp;
using CefSharp.WinForms;
using Microsoft.Win32;
using Models;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Services;
using Snippets;
using System;
using System.Collections;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
using WidgetsDotNet.Properties;

namespace Controllers
{
    internal class WidgetsManagerComponent : WidgetModel
    {
        private string widgetPath = String.Empty;
        private Form _window;
        private ChromiumWebBrowser _browser;
        private IntPtr _handle;
        private string managerUIPath = AssetService.assetsPath + "/index.html";
        private RegistryKey registryKey = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", true);
        private NotifyIcon notifyIcon;
        private ConfigurationModel appConfig;
        private JObject versionObject;
        private ResourceService resourceService = new ResourceService();
        private FormService formService = new FormService();
        private HTMLDocService HTMLDocService = new HTMLDocService();

        public override Form window
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

        public WidgetsManagerComponent()
        {
            this.OnInitialize();
        }

        private void OnInitialize()
        {
            CefSettings options = new CefSettings();
            options.CefCommandLineArgs.Add("disable-web-security");
            Cef.Initialize(options);

            AssetService.CreateHTMLFilesDirectory();
            PrepareRemoteResources();

            string json = File.ReadAllText(AssetService.assetsPath + "/config.json");
            appConfig = JsonConvert.DeserializeObject<ConfigurationModel>(json);

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
            CreateWindow(width, height, "WinWidgets", FormStartPosition.CenterScreen);
        }

        public void PrepareRemoteResources()
        {
            this.resourceService.DownloadRemoteResources();
        }

        public override void CreateWindow(int w, int h, string t, FormStartPosition p)
        {
            window = new Form();
            window.Size = new Size(w, h);
            window.StartPosition = p;
            window.Text = t;
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

        private async void ReloadWidgets()
        {
            versionObject = await this.resourceService.GetRemoteVersion();

            string template = 
                $"var container = document.getElementById('widgets');"
                + $"container.innerHTML = '';"
                + $"fetchedVersion = '{(string)versionObject["version"]}';"
                + $"isUpToDate = {(appConfig.version == (string)versionObject["version"] ? "true" : "false")};"
                + $"downloadUrl = '{(string)versionObject["downloadUrl"]}';"
                + "var options = {weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' };"
                + "var today = new Date();"
                + "updateCheckTime = today.toLocaleDateString();"
                + "document.getElementById('folder').onclick = () => CefSharp.PostMessage('widgetsFolder');"
                + "var switches = document.getElementsByClassName('switch');"
                + "for (let s of switches) {"
                + "const setting = s.getAttribute('setting');"
                + "if (setting == 'startup') {"
                + $"{(registryKey.GetValue("WinWidgets") != null ? "s.classList.add('switchon');" : "")}"
                + "}}";
            string[] files = AssetService.GetPathToHTMLFiles(AssetService.widgetsPath);

            for (int i = 0; i < files.Length; i++)
            {
                widgetPath = files[i];
                string localWidgetPath = widgetPath.Replace('\\', '/');

                template += $@"
                    var e{i} = document.createElement('div');"
                    + $"e{i}.classList.add('widget');"
                    + $"e{i}.classList.add('flex-row');"
                    + $"e{i}.style.width = '{(this.HTMLDocService.GetMetaTagValue("previewSize", widgetPath) != null ? this.HTMLDocService.GetMetaTagValue("previewSize", widgetPath).Split(' ')[0] : null)}px';"
                    + $"e{i}.style.minHeight = '{(this.HTMLDocService.GetMetaTagValue("previewSize", widgetPath) != null ? this.HTMLDocService.GetMetaTagValue("previewSize", widgetPath).Split(' ')[1] : null)}px';"
                    + $"e{i}.setAttribute('name', '{this.HTMLDocService.GetMetaTagValue("applicationTitle", widgetPath)}');"
                    + $"e{i}.innerHTML = `<p>{this.HTMLDocService.GetMetaTagValue("applicationTitle", widgetPath)}</p> <iframe src='file:///{localWidgetPath}'></iframe>`;"
                    + $"document.getElementById('widgets').appendChild(e{i});"
                    + $"e{i}.onclick = () => CefSharp.PostMessage('{i}');"
                 ;
            }

            browser.ExecuteScriptAsyncWhenPageLoaded(template);
        }

        public override void OpenWidget(int id)
        {
            WidgetComponent widget = new WidgetComponent();
            AssetService.widgets.AddWidget(widget);
            widget.widgetPath = AssetService.GetPathToHTMLFiles(AssetService.widgetsPath)[id];
            widget.CreateWindow(300, 300, $"Widget{id}", FormStartPosition.Manual);
        }

        private void OnStopAllWidgets(object sender, EventArgs e)
        {
            ArrayList deleteWidgets = new ArrayList();

            for (int i = 0; i < AssetService.widgets.Widgets.Count; i++)
            {
                ((WidgetComponent)AssetService.widgets.Widgets[i]).window.Invoke(new MethodInvoker(delegate ()
                {
                    ((WidgetComponent)AssetService.widgets.Widgets[i]).window.Close();
                    deleteWidgets.Add(((WidgetComponent)AssetService.widgets.Widgets[i]));
                }));
            }

            for (int i = 0; i < deleteWidgets.Count; i++)
            {
                AssetService.widgets.RemoveWidget((WidgetComponent)deleteWidgets[i]);
            }
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

            UserActivityHook userActivityHook = new UserActivityHook();
            userActivityHook.KeyDown += new KeyEventHandler(OnKeyDown);
            userActivityHook.KeyUp += new KeyEventHandler(OnKeyUp);
            userActivityHook.OnMouseActivity += new MouseEventHandler(OnMouseActivity);
            ReloadWidgets();
        }

        private void SendNativeKeyEvents(string data)
        {
            foreach (WidgetComponent widget in AssetService.widgets.Widgets)
            {
                widget.browser.ExecuteScriptAsync("if (typeof onNativeKeyEvents === 'function') { onNativeKeyEvents(" + data + "); }");
            }
        }

        private void OnKeyDown(object sender, KeyEventArgs e)
        {
            SendNativeKeyEvents(@"{ keyCode: " + e.KeyValue + ", state: \"keyDown\", eventType: \"keyboardEvent\" }");
        }

        private void OnKeyUp(object sender, KeyEventArgs e)
        {
            SendNativeKeyEvents(@"{ keyCode: " + e.KeyValue + ", state: \"keyUp\", eventType: \"keyboardEvent\" }");
        }

        private void OnMouseActivity(object sender, MouseEventArgs e)
        {
            SendNativeKeyEvents("{ eventType: \"mouseEvent\", xPosition: \"" + e.X + "\", yPosition: \"" + e.Y + "\", buttonPressed: \"" + e.Button + "\", buttonPressCount: \"" + e.Clicks + "\", mouseWheelOffset: \"" + e.Delta + "\" }");
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

                case "update":
                    Process.Start((string)versionObject["downloadUrl"]);
                    break;

                default:
                    OpenWidget(int.Parse((string)e.Message));
                    break;
            }
        }
    }
}