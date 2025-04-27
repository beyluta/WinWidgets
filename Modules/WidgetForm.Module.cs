using System;
using System.Timers;
using System.Windows.Forms;

namespace Modules
{
    internal class WidgetForm : Form
    {
        private readonly bool isWidget;
        private readonly bool enableDropShadow;

        /// <summary>
        /// Constructs a new instance of the WidgetForm class
        /// </summary>
        /// <param name="isWidget">Is the form a widget</param>
        public WidgetForm(bool enableDropShadow = false, bool isWidget = true) : base()
        {
            this.isWidget = isWidget;
            this.enableDropShadow = enableDropShadow;
        }

        protected override CreateParams CreateParams
        {
            get
            {
                const int CS_DROPSHADOW = 0x20000;
                CreateParams cp = base.CreateParams;
                if (enableDropShadow)
                cp.ClassStyle |= CS_DROPSHADOW;
                return cp;
            }
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
