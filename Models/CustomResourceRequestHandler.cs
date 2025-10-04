using CefSharp;
using CefSharp.Handler;

namespace WidgetsDotNet.Models
{
    internal class CustomResourceRequestHandler : ResourceRequestHandler
    {
        private readonly string userAgent;

        public CustomResourceRequestHandler(string userAgent)
        {
            this.userAgent = userAgent;
        }

        protected override CefReturnValue OnBeforeResourceLoad(IWebBrowser chromiumWebBrowser, IBrowser browser, IFrame frame, IRequest request, IRequestCallback callback)
        {
            var headers = request.Headers;
            headers["User-Agent"] = userAgent;
            request.Headers = headers;
            return base.OnBeforeResourceLoad(chromiumWebBrowser, browser, frame, request, callback);
        }
    }
}
