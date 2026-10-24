using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace HubUi {

public static class Theme {
  public static Color Bg      = Color.FromArgb(18, 19, 26);
  public static Color Panel   = Color.FromArgb(26, 28, 38);
  public static Color Panel2  = Color.FromArgb(34, 37, 50);
  public static Color Line    = Color.FromArgb(48, 52, 68);
  public static Color Text    = Color.FromArgb(236, 238, 245);
  public static Color Muted   = Color.FromArgb(132, 140, 162);
  public static Color Accent  = Color.FromArgb(0, 162, 255);
  public static Color Accent2 = Color.FromArgb(120, 92, 255);
  public static Color Good    = Color.FromArgb(58, 206, 130);
  public static Color Warn    = Color.FromArgb(245, 178, 64);
  public static Color Bad     = Color.FromArgb(240, 96, 96);

  public static GraphicsPath Round(Rectangle r, int rad) {
    GraphicsPath p = new GraphicsPath();
    if (rad <= 0) { p.AddRectangle(r); return p; }
    int d = rad * 2;
    if (d > r.Width)  d = r.Width;
    if (d > r.Height) d = r.Height;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
    p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
    p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
    p.CloseFigure();
    return p;
  }
}

public class Card : Panel {
  public int Radius = 11;
  public Color AccentStripe = Color.Transparent;

  public Card() {
    SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint |
             ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
    BackColor = Theme.Panel;
  }
  protected override void OnPaintBackground(PaintEventArgs e) { }
  protected override void OnPaint(PaintEventArgs e) {
    Graphics g = e.Graphics;
    g.SmoothingMode = SmoothingMode.AntiAlias;
    Rectangle r = new Rectangle(0, 0, Width - 1, Height - 1);
    using (GraphicsPath p = Theme.Round(r, Radius))
    using (SolidBrush b = new SolidBrush(Theme.Panel))
    using (Pen pen = new Pen(Theme.Line)) {
      g.FillPath(b, p);
      if (AccentStripe.A > 0) {
        g.SetClip(p);
        using (SolidBrush sb = new SolidBrush(AccentStripe))
          g.FillRectangle(sb, 0, 0, 4, Height);
        g.ResetClip();
      }
      g.DrawPath(pen, p);
    }
    base.OnPaint(e);
  }
}

public class PillBadge : Control {
  public Color PillColor = Theme.Good;
  public PillBadge() {
    SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint |
             ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
             ControlStyles.SupportsTransparentBackColor, true);
    BackColor = Color.Transparent;
    Height = 22;
  }
  protected override void OnPaint(PaintEventArgs e) {
    Graphics g = e.Graphics;
    g.SmoothingMode = SmoothingMode.AntiAlias;
    g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
    Rectangle r = new Rectangle(0, 0, Width - 1, Height - 1);
    Color fill = Color.FromArgb(36, PillColor.R, PillColor.G, PillColor.B);
    Color rim  = Color.FromArgb(115, PillColor.R, PillColor.G, PillColor.B);
    using (GraphicsPath p = Theme.Round(r, 11))
    using (SolidBrush b = new SolidBrush(fill))
    using (Pen pen = new Pen(rim)) {
      g.FillPath(b, p);
      g.DrawPath(pen, p);
    }
    using (SolidBrush dot = new SolidBrush(PillColor))
      g.FillEllipse(dot, 9, Height / 2 - 3, 6, 6);
    TextRenderer.DrawText(g, Text, Font, new Rectangle(19, 0, Width - 25, Height),
      PillColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
  }
}

public class Toggle : Control {
  bool _on = false, _hover = false;
  public event EventHandler CheckedChanged;
  public bool Checked {
    get { return _on; }
    set {
      if (_on != value) {
        _on = value;
        Invalidate();
        if (CheckedChanged != null) CheckedChanged(this, EventArgs.Empty);
      }
    }
  }
  public void SetQuiet(bool v) { _on = v; Invalidate(); }

  public Toggle() {
    SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint |
             ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
             ControlStyles.SupportsTransparentBackColor, true);
    BackColor = Color.Transparent;
    Height = 24;
    Cursor = Cursors.Hand;
  }
  protected override void OnClick(EventArgs e) { base.OnClick(e); Checked = !Checked; }
  protected override void OnMouseEnter(EventArgs e) { base.OnMouseEnter(e); _hover = true;  Invalidate(); }
  protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); _hover = false; Invalidate(); }

  protected override void OnPaint(PaintEventArgs e) {
    Graphics g = e.Graphics;
    g.SmoothingMode = SmoothingMode.AntiAlias;
    g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
    int h = 18, w = 34;
    int ty = (Height - h) / 2;
    Rectangle track = new Rectangle(0, ty, w, h);
    using (GraphicsPath p = Theme.Round(track, h / 2)) {
      if (_on) {
        using (LinearGradientBrush b = new LinearGradientBrush(track, Theme.Accent2, Theme.Accent, LinearGradientMode.Horizontal))
          g.FillPath(b, p);
      } else {
        using (SolidBrush b = new SolidBrush(_hover ? Theme.Line : Color.FromArgb(44, 47, 62)))
          g.FillPath(b, p);
        using (Pen pen = new Pen(Color.FromArgb(72, 78, 98)))
          g.DrawPath(pen, p);
      }
    }
    int kd = h - 6;
    int kx = _on ? (w - kd - 3) : 3;
    using (SolidBrush b = new SolidBrush(_on ? Color.White : Color.FromArgb(188, 194, 212)))
      g.FillEllipse(b, kx, ty + 3, kd, kd);

    Rectangle tr = new Rectangle(w + 9, 0, Width - w - 9, Height);
    TextRenderer.DrawText(g, Text, Font, tr, Theme.Muted,
      TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
  }
}

public class FlatBtn : Control {
  bool _hover = false, _down = false;
  public bool Primary = false;
  public FlatBtn() {
    SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint |
             ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
             ControlStyles.SupportsTransparentBackColor, true);
    BackColor = Color.Transparent;
    Height = 32;
    Cursor = Cursors.Hand;
  }
  protected override void OnMouseEnter(EventArgs e) { base.OnMouseEnter(e); _hover = true;  Invalidate(); }
  protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); _hover = false; _down = false; Invalidate(); }
  protected override void OnMouseDown(MouseEventArgs e) { base.OnMouseDown(e); _down = true;  Invalidate(); }
  protected override void OnMouseUp(MouseEventArgs e)   { base.OnMouseUp(e);   _down = false; Invalidate(); }

  protected override void OnPaint(PaintEventArgs e) {
    Graphics g = e.Graphics;
    g.SmoothingMode = SmoothingMode.AntiAlias;
    g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
    Rectangle r = new Rectangle(0, 0, Width - 1, Height - 1);
    using (GraphicsPath p = Theme.Round(r, 7)) {
      if (Primary) {
        Color a = Theme.Accent2, b2 = Theme.Accent;
        if (_down)       { a = Color.FromArgb(96, 74, 214); b2 = Color.FromArgb(0, 132, 210); }
        else if (_hover) { a = Color.FromArgb(136, 108, 255); b2 = Color.FromArgb(28, 178, 255); }
        using (LinearGradientBrush b = new LinearGradientBrush(new Rectangle(0, 0, Math.Max(Width, 1), Height), a, b2, LinearGradientMode.Horizontal))
          g.FillPath(b, p);
      } else {
        Color c = Theme.Panel2;
        if (_down)       c = Color.FromArgb(28, 30, 40);
        else if (_hover) c = Theme.Line;
        using (SolidBrush b = new SolidBrush(c)) g.FillPath(b, p);
        using (Pen pen = new Pen(Theme.Line)) g.DrawPath(pen, p);
      }
    }
    TextRenderer.DrawText(g, Text, Font, new Rectangle(0, 0, Width, Height),
      Primary ? Color.White : Theme.Text,
      TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
  }
}

public class TitleBar : Control {
  public event EventHandler CloseClicked;
  public event EventHandler MinClicked;
  int _hot = -1;
  Form _host;
  public Image Logo;

  public TitleBar(Form host) {
    _host = host;
    SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint |
             ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
    Height = 44;
    BackColor = Theme.Bg;
    Dock = DockStyle.Top;
  }

  Rectangle BtnRect(int i) {
    int w = 46;
    return new Rectangle(Width - w * (2 - i), 0, w, Height);
  }

  protected override void OnMouseMove(MouseEventArgs e) {
    base.OnMouseMove(e);
    int was = _hot; _hot = -1;
    for (int i = 0; i < 2; i++) if (BtnRect(i).Contains(e.Location)) _hot = i;
    if (was != _hot) Invalidate();
  }
  protected override void OnMouseLeave(EventArgs e) {
    base.OnMouseLeave(e);
    if (_hot != -1) { _hot = -1; Invalidate(); }
  }
  protected override void OnMouseDown(MouseEventArgs e) {
    base.OnMouseDown(e);
    if (e.Button != MouseButtons.Left) return;
    if (BtnRect(0).Contains(e.Location)) { if (MinClicked != null) MinClicked(this, EventArgs.Empty); return; }
    if (BtnRect(1).Contains(e.Location)) { if (CloseClicked != null) CloseClicked(this, EventArgs.Empty); return; }
    ReleaseCapture();
    SendMessage(_host.Handle, 0xA1, 0x2, 0);
  }

  [DllImport("user32.dll")] static extern bool ReleaseCapture();
  [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr hWnd, int Msg, int wParam, int lParam);

  protected override void OnPaint(PaintEventArgs e) {
    Graphics g = e.Graphics;
    g.SmoothingMode = SmoothingMode.AntiAlias;
    g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
    using (SolidBrush b = new SolidBrush(Theme.Bg)) g.FillRectangle(b, ClientRectangle);

    int x = 14;
    if (Logo != null) {
      g.DrawImage(Logo, new Rectangle(x, Height / 2 - 9, 18, 18));
      x += 26;
    }
    TextRenderer.DrawText(g, Text, Font, new Rectangle(x, 0, Width - x - 100, Height),
      Theme.Text, TextFormatFlags.Left | TextFormatFlags.VerticalCenter);

    for (int i = 0; i < 2; i++) {
      Rectangle r = BtnRect(i);
      if (_hot == i) {
        using (SolidBrush b = new SolidBrush(i == 1 ? Color.FromArgb(232, 68, 68) : Theme.Panel2))
          g.FillRectangle(b, r);
      }
      Color ic = (_hot == i && i == 1) ? Color.White : Theme.Muted;
      using (Pen pen = new Pen(ic, 1.4f)) {
        pen.StartCap = LineCap.Round; pen.EndCap = LineCap.Round;
        int cx = r.X + r.Width / 2, cy = r.Height / 2;
        if (i == 0) g.DrawLine(pen, cx - 5, cy + 3, cx + 5, cy + 3);
        else { g.DrawLine(pen, cx - 5, cy - 5, cx + 5, cy + 5);
               g.DrawLine(pen, cx + 5, cy - 5, cx - 5, cy + 5); }
      }
    }
    using (Pen pen = new Pen(Theme.Line))
      g.DrawLine(pen, 0, Height - 1, Width, Height - 1);
  }
}

public static class Dpi {
  [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
  [DllImport("shcore.dll")] static extern int SetProcessDpiAwareness(int value);
  [DllImport("user32.dll")] static extern bool SetProcessDpiAwarenessContext(IntPtr value);

  public static void Enable() {
    try { if (SetProcessDpiAwarenessContext(new IntPtr(-4))) return; } catch { }
    try { if (SetProcessDpiAwareness(2) == 0) return; } catch { }
    try { SetProcessDPIAware(); } catch { }
  }
}

public static class Win {
  [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int val, int size);
  [DllImport("user32.dll", CharSet = CharSet.Auto)] static extern IntPtr SendMessage(IntPtr hwnd, int msg, IntPtr wp, IntPtr lp);
  [DllImport("shell32.dll", CharSet = CharSet.Unicode)] static extern int SetCurrentProcessExplicitAppUserModelID(string id);

  public static void SetAppId(string id) { try { SetCurrentProcessExplicitAppUserModelID(id); } catch { } }

  public static void SetIcon(IntPtr hwnd, IntPtr hIcon) {
    if (hwnd == IntPtr.Zero || hIcon == IntPtr.Zero) return;
    try {
      SendMessage(hwnd, 0x0080, (IntPtr)0, hIcon);
      SendMessage(hwnd, 0x0080, (IntPtr)1, hIcon);
      SendMessage(hwnd, 0x0080, (IntPtr)2, hIcon);
    } catch { }
  }

  public static void RoundCorners(IntPtr hwnd) {
    try { int pref = 2; DwmSetWindowAttribute(hwnd, 33, ref pref, sizeof(int)); } catch { }
    try { int dark = 1; DwmSetWindowAttribute(hwnd, 20, ref dark, sizeof(int)); } catch { }
  }
}

}
