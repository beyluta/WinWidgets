using CefSharp;
using CefSharp.WinForms;
using Microsoft.Win32;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Remote;
using Snippets;
using System;
using System.Collections;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net.Http;
using System.Windows.Forms;
using WidgetsDotNet.Properties;

namespace Widgets.Manager
{
    internal class WidgetsManager : WidgetWindow
    {
        private string widgetPath = String.Empty;
        private Form _window;
        private ChromiumWebBrowser _browser;
        private IntPtr _handle;
        private string managerUIPath = WidgetAssets.assetsPath + "/index.html";
        private RegistryKey registryKey = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", true);
        private NotifyIcon notifyIcon;
        private Configuration appConfig;
        private JObject versionObject;

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

        public WidgetsManager()
        {
            /*
            @@  CefSharp configurations.
            */
            CefSettings options = new CefSettings();
            options.CefCommandLineArgs.Add("disable-web-security");
            Cef.Initialize(options);

            /*
            @@   Creating the default folder C://USERS/MY_USER/Widgets
            */
            WidgetAssets.CreateHTMLFilesDirectory();

            /*
            @@  Fetching online resources such as standard widgets and software version
            */
            PrepareRemoteResources();

            /*
            @@  Loading the configurations of this software into the appConfig class instance.
            */
            string json = File.ReadAllText(WidgetAssets.assetsPath + "/config.json");
            appConfig = JsonConvert.DeserializeObject<Configuration>(json);

            /*
            @@  Notify Icon at the bottom right.
            @@
            @@  It has some useful settings that can be accessed quickly.
            */
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

            /*
            @@  Finally, creating the widget manager window.
            */
            Rectangle screenResolution = Screen.PrimaryScreen.Bounds;
            int width  = screenResolution.Width / 2;
            int height = screenResolution.Height - 200;
            CreateWindow(width, height, "WinWidgets", FormStartPosition.CenterScreen);
        }

        public async void PrepareRemoteResources()
        {
            using (HttpClient client = new HttpClient())
            {
                versionObject = JObject.Parse("{\"version\":\"" + appConfig.version + "\",\"downloadUrl\":\"https://github.com/beyluta/WinWidgets\"}");

                try
                {
                    string response = await client.GetStringAsync("https://7xdeveloper.com/api/AccessEndpoint.php?endpoint=getappconfigs&id=version");
                    versionObject = JObject.Parse(response);
                }
                catch { }
            }

            RemoteResources remoteResources = new RemoteResources();
            remoteResources.DownloadRemoteResources();
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

        private void ReloadWidgets()
        {
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
            string[] files = WidgetAssets.GetPathToHTMLFiles(WidgetAssets.widgetsPath);

            for (int i = 0; i < files.Length; i++)
            {
                widgetPath = files[i];
                string localWidgetPath = widgetPath.Replace('\\', '/');

                template += $@"
                    var e{i} = document.createElement('div');"
                    + $"e{i}.classList.add('widget');"
                    + $"e{i}.classList.add('flex-row');"
                    + $"e{i}.style.width = '{(GetMetaTagValue("previewSize", widgetPath) != null ? GetMetaTagValue("previewSize", widgetPath).Split(' ')[0] : null)}px';"
                    + $"e{i}.style.minHeight = '{(GetMetaTagValue("previewSize", widgetPath) != null ? GetMetaTagValue("previewSize", widgetPath).Split(' ')[1] : null)}px';"
                    + $"e{i}.setAttribute('name', '{GetMetaTagValue("applicationTitle", widgetPath)}');"
                    + $"e{i}.innerHTML = `<p>{GetMetaTagValue("applicationTitle", widgetPath)}</p> <iframe src='file:///{localWidgetPath}'></iframe>`;"
                    + $"document.getElementById('widgets').appendChild(e{i});"
                    + $"e{i}.onclick = () => CefSharp.PostMessage('{i}');"
                 ;
            }

            browser.ExecuteScriptAsyncWhenPageLoaded(template);
        }

        public override void OpenWidget(int id)
        {
            Widget widget = new Widget();
            WidgetAssets.widgets.AddWidget(widget);
            widget.widgetPath = WidgetAssets.GetPathToHTMLFiles(WidgetAssets.widgetsPath)[id];
            widget.CreateWindow(300, 300, $"Widget{id}", FormStartPosition.Manual);
        }

        private void OnStopAllWidgets(object sender, EventArgs e)
        {
            ArrayList deleteWidgets = new ArrayList();

            for (int i = 0; i < WidgetAssets.widgets.Widgets.Count; i++)
            {
                ((Widget)WidgetAssets.widgets.Widgets[i]).window.Invoke(new MethodInvoker(delegate ()
                {
                    ((Widget)WidgetAssets.widgets.Widgets[i]).window.Close();
                    deleteWidgets.Add(((Widget)WidgetAssets.widgets.Widgets[i]));
                }));
            }

            for (int i = 0; i < deleteWidgets.Count; i++)
            {
                WidgetAssets.widgets.RemoveWidget((Widget)deleteWidgets[i]);
            }
        }

        private void OnFormResized(object sender, EventArgs e)
        {
            if (window.WindowState == FormWindowState.Minimized)
            {
                window.Opacity = 0;
            }
        }

        private void OnOpenApplication(object sender, EventArgs e)
        {
            window.Opacity = 100;
            window.WindowState = FormWindowState.Normal;
        }

        private void NotifyIconDoubleClick(object sender, MouseEventArgs e)
        {
            window.Opacity = 100;
            window.WindowState = FormWindowState.Normal;
        }

        private void OnBrowserInitialized(object sender, EventArgs e)
        {
            FileSystemWatcher fileWatcher = new FileSystemWatcher(WidgetAssets.widgetsPath);
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
            foreach (Widget widget in WidgetAssets.widgets.Widgets)
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
            /*
            @@  Receives messages from the JavaScript.
            @@
            @@  If the message is a number then it defaults to opening a widget by its id.
            */
            switch (e.Message)
            {
                case "widgetsFolder":
                    Process.Start(WidgetAssets.widgetsPath);
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