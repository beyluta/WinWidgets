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
        private IntPtr _handle;
        private string _widgetPath;
        private Form _window;
        private ChromiumWebBrowser _browser;
        private bool allowWidgetSettingsOverlay = true;
        private bool isMouseDown = false;
        private bool isMouseOver = false;
        private int width;
        private int height;

        public Widget(int id)
        {
            this.id = id;
        }

        public Widget() { }

        public override IntPtr handle
        {
            get { return _handle; }
            set { _handle = value; }
        }


        public string widgetPath 
        { 
            get { return _widgetPath; }
            set { _widgetPath = value; }
        }

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

        public override void AppendWidget(Form f, string path)
        {
            browser = new ChromiumWebBrowser(path);
            browser.IsBrowserInitializedChanged += OnBrowserInitialized;
            f.Controls.Add(browser);
        }

        public override void CreateWindow(int w, int h, string t, FormStartPosition p)
        {
            string sizeString = GetMetaTagValue("windowSize", widgetPath);
            string radiusString = GetMetaTagValue("windowBorderRadius", widgetPath);
            string locationString = GetMetaTagValue("windowLocation", widgetPath);
            string topMostString = GetMetaTagValue("topMost", widgetPath);
            string overlay = GetMetaTagValue("windowOverlay", widgetPath);
            int roundess = radiusString != null ? int.Parse(radiusString) : 0;
            width = sizeString != null ? int.Parse(sizeString.Split(' ')[0]) : w;
            height = sizeString != null ? int.Parse(sizeString.Split(' ')[1]) : h;
            int locationX = locationString != null ? int.Parse(locationString.Split(' ')[0]) : 0;
            int locationY = locationString != null ? int.Parse(locationString.Split(' ')[1]) : 0;
            bool topMost = topMostString != null ? bool.Parse(topMostString.Split(' ')[0]) : false;
            allowWidgetSettingsOverlay = overlay == "true" ? true : false;

            window = new Form();
            window.Size = new Size(width, height);
            window.StartPosition = p;
            window.Location = new Point(locationX, locationY);
            window.Text = t;
            window.TopMost = topMost;
            window.FormBorderStyle = FormBorderStyle.None;
            window.ShowInTaskbar = false;
            window.Region = System.Drawing.Region.FromHrgn(CreateRoundRectRgn(0, 0, width, height, roundess, roundess));
            window.Activated += OnFormActivated;
            AppendWidget(window, widgetPath);
            window.ShowDialog();
        }

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
                    var mouseDrag;
                    document.body.onmousedown = () => {
                        mouseDrag = setInterval(() => {
                            CefSharp.PostMessage('mouseDrag');
                        }, 0);
                        CefSharp.PostMessage('onmousedown');
                    }
                    document.body.onmouseup = () => {
                         clearInterval(mouseDrag);
                         CefSharp.PostMessage('onmouseup');
                    }
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

                case "mouseDrag":
                    POINT pos;
                    GetCursorPos(out pos);
                    SetWindowPos(handle, 0, pos.X - width / 2, pos.Y - height / 2, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW);
                    break;
            }
        }

        private void OnFormActivated(object sender, EventArgs e)
        {
            handle = window.Handle;
            browser.MenuHandler = new WidgetMenuHandler(handle);
        }

        public override void OpenWidget(int id)
        {
            throw new System.NotImplementedException();
        }
    }
}
