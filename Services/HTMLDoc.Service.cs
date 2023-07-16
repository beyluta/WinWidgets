namespace Services
{
    internal class HTMLDocService
    {
        /// <summary>
        /// Searches for the value of a meta tag by name
        /// </summary>
        /// <param name="name">Name of the tag</param>
        /// <param name="widgetPath">Path to the file</param>
        /// <returns>The value inside the content attribute</returns>
        public string GetMetaTagValue(string name, string widgetPath)
        {
            try
            {
                var doc = new HtmlAgilityPack.HtmlDocument();
                doc.Load(widgetPath);
                return doc.DocumentNode.SelectSingleNode("//meta[@name='" + name + "']")?.GetAttributeValue("content", null);
            }
            catch { return null; }
        }
    }
}
