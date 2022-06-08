using CefSharp;
using CefSharp.WinForms;
using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace Widgets
{
    class Widget : WidgetWindow
    {
        private int id;

        public Widget(int id)
        {
            this.id = id;
        }

        public Widget() { }

        IntPtr _handle;

        public override IntPtr handle
        {
            get { return _handle; }
            set { _handle = value; }
        }

        private string _widgetPath;

        public string widgetPath 
        { 
            get { return _widgetPath; }
            set { _widgetPath = value; }
        }

        private Form _window;

        public override Form window
        {
            get { return _window; }
            set { _window = value; }
        }

        private ChromiumWebBrowser _browser;

        public override ChromiumWebBrowser browser
        {
            get { return _browser; }
            set { _browser = value; }
        }

        public override void AppendWidget(Form f, string path)
        {
            browser = new ChromiumWebBrowser(path);
            browser.IsBrowserInitializedChanged += OnBrowserInitialized;
            f.Controls.Add(browser);
        }

        bool allowWidgetSettingsOverlay = true;

        public override void CreateWindow(int w, int h, string t, FormStartPosition p)
        {
            string sizeString = GetMetaTagValue("windowSize");
            string radiusString = GetMetaTagValue("windowBorderRadius");
            string locationString = GetMetaTagValue("windowLocation");
            string topMostString = GetMetaTagValue("topMost");
            string overlay = GetMetaTagValue("windowOverlay");
            int roundess = radiusString != null ? int.Parse(radiusString) : 0;
            int metaWidth = sizeString != null ? int.Parse(sizeString.Split(' ')[0]) : w;
            int metaHeight = sizeString != null ? int.Parse(sizeString.Split(' ')[1]) : h;
            int locationX = locationString != null ? int.Parse(locationString.Split(' ')[0]) : 0;
            int locationY = locationString != null ? int.Parse(locationString.Split(' ')[1]) : 0;
            bool topMost = topMostString != null ? bool.Parse(topMostString.Split(' ')[0]) : false;
            allowWidgetSettingsOverlay = overlay == "true" ? true : false;

            window = new Form();
            window.Size = new Size(metaWidth, metaHeight);
            window.StartPosition = p;
            window.Location = new Point(locationX, locationY);
            window.Text = t;
            window.TopMost = topMost;
            window.FormBorderStyle = FormBorderStyle.None;
            window.ShowInTaskbar = false;
            window.Region = System.Drawing.Region.FromHrgn(CreateRoundRectRgn(0, 0, metaWidth, metaHeight, roundess, roundess));
            window.Activated += OnFormActivated;
            AppendWidget(window, widgetPath);
            window.ShowDialog();
        }

        private bool isMouseDown = false;
        private bool isMouseOver = false;

        private void OnBrowserInitialized(object sender, EventArgs e)
        {
            if (allowWidgetSettingsOverlay)
            {
                browser.ExecuteScriptAsync(@"
                const wrapper = document.createElement('div');
                wrapper.setAttribute('id', 'browserWrapper');
                wrapper.style.position = 'fixed';
                wrapper.style.background = 'rgba(1, 1, 1, .98)';
                wrapper.style.width = '100%';
                wrapper.style.height = '100%';
                wrapper.style.zIndex = '999';
                wrapper.style.display = 'none';

                const del = document.createElement('div');
                del.setAttribute('id', 'wrapperDeleteIcon');
                del.style.position = 'fixed';
                del.style.margin = '0';
                del.style.top = '50%';
                del.style.left = '50%';
                del.style.padding = '20px';
                del.style.borderRadius = '10px';
                del.style.background = 'rgba(255, 255, 255, 0.1)';
                del.style.transform = 'translate(-50%, -50%)';
                del.style.cursor = 'pointer';
                del.innerHTML = `<svg width='20px' fill='white' viewBox='0 0 448 512'><path d='M135.2 17.69C140.6 6.848 151.7 0 163.8 0H284.2C296.3 0 307.4 6.848 312.8 17.69L320 32H416C433.7 32 448 46.33 448 64C448 81.67 433.7 96 416 96H32C14.33 96 0 81.67 0 64C0 46.33 14.33 32 32 32H128L135.2 17.69zM394.8 466.1C393.2 492.3 372.3 512 346.9 512H101.1C75.75 512 54.77 492.3 53.19 466.1L31.1 128H416L394.8 466.1z'/></svg>`;
                wrapper.appendChild(del);
                document.body?.appendChild(wrapper);
                
                const interval = setInterval(() => {
                    if (document.body)
                    {
                        document.body.appendChild(wrapper);
                        createEvents();
                        clearInterval(interval);
                    }
                }, 0);

                const createEvents = () => {
                    document.body.onmouseleave = () => { 
                        document.getElementById('browserWrapper').style.display = 'none';
                        CefSharp.PostMessage('onmouseleave');
                    }
                    document.body.onmouseenter = () => { 
                        document.getElementById('browserWrapper').style.display = 'block';                
                        CefSharp.PostMessage('onmouseenter');
                    }
                    document.body.onmousedown = () => CefSharp.PostMessage('onmousedown');
                    document.body.onmouseup = () => CefSharp.PostMessage('onmouseup');
                    document.getElementById('wrapperDeleteIcon').onclick = () => CefSharp.PostMessage('deletewidget');
                }
            ");
            }
        
            browser.JavascriptMessageReceived += OnBrowserMessageReceived;
        }

        private void OnBrowserMessageReceived(object sender, JavascriptMessageReceivedEventArgs e)
        {
            switch (e.Message)
            {
                case "onmouseleave":
                    isMouseOver = false;
                    break;

                case "onmouseenter":
                    isMouseOver = true;
                    break;

                case "onmousedown":
                    isMouseDown = true;
                    break;

                case "onmouseup":
                    isMouseDown = false;
                    break;

                case "deletewidget":
                    SendMessage(handle, WM_CLOSE, IntPtr.Zero, IntPtr.Zero);
                    break;
            }
        }

        private void OnFormActivated(object sender, EventArgs e)
        {
            handle = window.Handle;
            browser.MenuHandler = new WidgetMenuHandler(handle);
        }

        public override string GetMetaTagValue(string name)
        {
            string[] html = File.ReadAllLines(widgetPath);
            for (int i = 0; i < html.Length; i++)
            {
                if (html[i].Contains("meta") && html[i].Contains(name) && !html[i].Contains("<!--"))
                {
                    return html[i].Split('"')[3];
                }
            }
            return null;
        }

        public override void OpenWidget(int id)
        {
            throw new System.NotImplementedException();
        }

        public override void SetWindowTransparency(IntPtr handle, byte alpha)
        {
            SetWindowLong(handle, GWL_EXSTYLE, GetWindowLong(handle, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(handle, 0, alpha, LWA_ALPHA);
        }
    }
}
