using Services;

namespace Service
{
    internal class TemplateService
    {
        /// <summary>
        /// Moves all templates in the templates path to the widgets folder
        /// </summary>
        public void MoveTemplatesToWidgetsPath()
        {
            AssetService.MoveFilesToPath(AssetService.templatePath, AssetService.widgetsPath);
        }
    }
}
