using System;
using System.Drawing;
using System.Windows.Forms;

namespace WidgetsDotNet.Models
{
    internal class FormModel : Form
    {
        public delegate void MouseMovedEvent();

        public class GlobalMouseHandler : IMessageFilter
        {
            private const int WM_MOUSEMOVEDOWN = 0x0201;

            public event MouseMovedEvent TheMouseMoved;

            #region IMessageFilter Members

            public bool PreFilterMessage(ref Message m)
            {
                Console.WriteLine(m.Msg);

                if (m.Msg == WM_MOUSEMOVEDOWN)
                {
                    Console.WriteLine(m.Msg.ToString());

                    if (TheMouseMoved != null)
                    {
                        TheMouseMoved();
                    }
                }
                // Always allow message to continue to the next filter control
                return false;
            }

            #endregion
        }

        public FormModel() : base()
        {
            GlobalMouseHandler gmh = new GlobalMouseHandler();
            gmh.TheMouseMoved += new MouseMovedEvent(gmh_TheMouseMoved);
            Application.AddMessageFilter(gmh);
        }

        void gmh_TheMouseMoved()
        {
            Console.WriteLine("works");
            Point cur_pos = System.Windows.Forms.Cursor.Position;
            System.Console.WriteLine(cur_pos);
        }
    }
}
