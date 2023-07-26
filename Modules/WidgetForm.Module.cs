using System;
using System.Timers;
using System.Windows.Forms;

namespace Modules
{
    internal class WidgetForm : Form
    {
        private bool isWidget;

        /// <summary>
        /// Constructs a new instance of the WidgetForm class
        /// </summary>
        /// <param name="isWidget">Is the form a widget</param>
        public WidgetForm(bool isWidget = true) : base()
        {
            this.isWidget = isWidget;
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            base.OnFormClosing(e);

            if (!isWidget && e.CloseReason == CloseReason.UserClosing)
            {
                e.Cancel = true;
                this.WindowState = FormWindowState.Minimized;
            }
        }
    }
}
