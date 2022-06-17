using CefSharp;
using CefSharp.WinForms;
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace Widgets
{
    class Widget : WidgetWindow
    {
        public bool moveModeEnabled = false;

        private IntPtr _handle;
        private string _widgetPath;
        private Form _window;
        private ChromiumWebBrowser _browser;
        private int width;
        private int height;

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

        public override void CreateWindow(int width, int height, string title, FormStartPosition startPosition)
        {
            POINT mousePos;
            GetCursorPos(out mousePos);
            string sizeString = GetMetaTagValue("windowSize", widgetPath);
            string radiusString = GetMetaTagValue("windowBorderRadius", widgetPath);
            string locationString = GetMetaTagValue("windowLocation", widgetPath);
            string topMostString = GetMetaTagValue("topMost", widgetPath);
            string opacityString = GetMetaTagValue("windowOpacity", widgetPath);
            int roundess = radiusString != null ? int.Parse(radiusString) : 0;
            this.width = sizeString != null ? int.Parse(sizeString.Split(' ')[0]) : width;
            this.height = sizeString != null ? int.Parse(sizeString.Split(' ')[1]) : height;
            int locationX = locationString != null ? int.Parse(locationString.Split(' ')[0]) : mousePos.X;
            int locationY = locationString != null ? int.Parse(locationString.Split(' ')[1]) : mousePos.Y;
            byte opacity = (byte)(opacityString != null ? byte.Parse(opacityString.Split(' ')[0]) : 255);
            bool topMost = topMostString != null ? bool.Parse(topMostString.Split(' ')[0]) : false;

            window = new Form();
            window.Size = new Size(this.width, this.height);
            window.StartPosition = startPosition;
            window.Location = new Point(locationX, locationY);
            window.Text = title;
            window.TopMost = topMost;
            window.FormBorderStyle = FormBorderStyle.None;
            window.ShowInTaskbar = false;
            window.Region = System.Drawing.Region.FromHrgn(CreateRoundRectRgn(0, 0, this.width, this.height, roundess, roundess)); // Border radius
            window.Activated += OnFormActivated;
            window.BackColor = Color.Black;

            /*
            @@  The code below is setting the window to have a blurred background.
            @@  It does not work as of yet because this software uses winforms as its base. But it will be changed to WPF soon.
            */
            AccentPolicy accentPolicy = new AccentPolicy
            {
                AccentState = AccentState.ACCENT_ENABLE_BLURBEHIND,
            };
            int accentSize = Marshal.SizeOf(accentPolicy);
            IntPtr accentPtr = Marshal.AllocHGlobal(accentSize);
            Marshal.StructureToPtr(accentPolicy, accentPtr, false);
            WindowCompositionAttributeData data = new WindowCompositionAttributeData
            {
                Attribute = WindowCompositionAttribute.WCA_ACCENT_POLICY,
                SizeOfData = accentSize,
                Data = accentPtr
            };
            SetWindowCompositionAttribute(window.Handle, ref data);

            SetWindowTransparency(window.Handle, opacity);
            AppendWidget(window, widgetPath);
            window.ShowDialog();
        }

        private void OnBrowserInitialized(object sender, EventArgs e)
        {
            browser.ExecuteScriptAsync(@"
                window.onload = () => {
                    let mouseDrag;

                    document.body.onmousedown = (e) => {
                        mouseDrag = setInterval(() => {
                            e.buttons === 1 && CefSharp.PostMessage('mouseDrag');
                        }, 0);
                        CefSharp.PostMessage('onmousedown');
                    }

                    document.body.onmouseup = () => {
                         clearInterval(mouseDrag);
                         CefSharp.PostMessage('onmouseup');
                    }
                }
            ");

            browser.JavascriptMessageReceived += OnBrowserMessageReceived;
        }

        public override void OpenWidget(int id)
        {
            throw new System.NotImplementedException();
        }

        private void OnBrowserMessageReceived(object sender, JavascriptMessageReceivedEventArgs e)
        {
            switch (e.Message)
            {
                case "mouseDrag":
                    if (moveModeEnabled)
                    {
                        POINT pos;
                        GetCursorPos(out pos);
                        window.Invoke(new MethodInvoker(delegate ()
                        {
                            window.Location = new Point(pos.X - width / 2, pos.Y - height / 2);
                        }));
                    }
                    break;
            }
        }

        private void OnFormActivated(object sender, EventArgs e)
        {
            handle = window.Handle;
            browser.MenuHandler = new WidgetMenuHandler(this);
        }
    }
}
