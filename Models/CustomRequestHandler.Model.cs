using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using CefSharp;

namespace WidgetsDotNet.Models
{
    public class CustomRequestHandler : IRequestHandler
    {
        private readonly string _userAgent = "WinWidgetsUserAgent";

        private readonly List<string> _excludedProtocols = new List<string>() { "http", "https", "file" };

        public bool OnBeforeBrowse(IWebBrowser chromium, IBrowser browser, IFrame frame, IRequest request, bool gesture, bool redirect)
        {
            string protocol = request.Url.Split(':')[0];

            if (_excludedProtocols.Contains(protocol))
            {
                return false;
            }

            if (request.Url.Contains(':'))
            {
                Process.Start(new ProcessStartInfo(request.Url) { UseShellExecute = true });
                return true;
            }

            return false;
        }

        public IResourceRequestHandler GetResourceRequestHandler(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IRequest request, bool isNavigation, bool isDownload, string requestInitiator, ref bool disableDefaultHandling)
        {
            return new CustomResourceRequestHandler(_userAgent);
        }

        public bool GetAuthCredentials(IWebBrowser chromiumWebBrowser, IBrowser browser, string originUrl, bool isProxy, string host, int port, string realm, string scheme, IAuthCallback callback) => false;

        public bool OnCertificateError(IWebBrowser chromiumWebBrowser, IBrowser browser, CefErrorCode errorCode, string requestUrl, ISslInfo sslInfo, IRequestCallback callback) => false;

        public void OnDocumentAvailableInMainFrame(IWebBrowser chromiumWebBrowser, IBrowser browser) { }

        public bool OnOpenUrlFromTab(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, string targetUrl, WindowOpenDisposition targetDisposition, bool userGesture) => false;

        public void OnRenderProcessTerminated(IWebBrowser chromiumWebBrowser, IBrowser browser, CefTerminationStatus status, int errorCode, string errorMessage) { }

        public void OnRenderViewReady(IWebBrowser chromiumWebBrowser, IBrowser browser) { }

        public bool OnSelectClientCertificate(IWebBrowser chromiumWebBrowser, IBrowser browser, bool isProxy, string host, int port, X509Certificate2Collection certificates, ISelectClientCertificateCallback callback) => false;
    }
}
