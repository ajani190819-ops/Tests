using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

namespace BtGuard {

public class BtDeviceSnapshot {
  public ulong Address;
  public string Mac;
  public string Name;
  public string Kind;
  public uint ClassOfDevice;
  public bool Connected;
  public bool Remembered;
  public int InstalledServiceCount;
}

public static class BtNative {
  public const uint BLUETOOTH_SERVICE_DISABLE = 0x00;
  public const uint BLUETOOTH_SERVICE_ENABLE  = 0x01;
  public const uint IOCTL_BTH_DISCONNECT_DEVICE = 0x0041000C;

  public static readonly Guid GuidAudioSink      = new Guid("0000110B-0000-1000-8000-00805F9B34FB");
  public static readonly Guid GuidAvRemoteTarget = new Guid("0000110C-0000-1000-8000-00805F9B34FB");
  public static readonly Guid GuidAvRemote       = new Guid("0000110E-0000-1000-8000-00805F9B34FB");
  public static readonly Guid GuidHeadset        = new Guid("00001108-0000-1000-8000-00805F9B34FB");
  public static readonly Guid GuidHandsfree      = new Guid("0000111E-0000-1000-8000-00805F9B34FB");
  public static readonly Guid GuidGenericAudio   = new Guid("00001203-0000-1000-8000-00805F9B34FB");
  public static readonly Guid GuidHid            = new Guid("00001124-0000-1000-8000-00805F9B34FB");

  [StructLayout(LayoutKind.Sequential)]
  public struct SYSTEMTIME {
    public ushort wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
  }

  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  public struct BLUETOOTH_DEVICE_INFO {
    public uint dwSize;
    public ulong Address;
    public uint ulClassofDevice;
    [MarshalAs(UnmanagedType.Bool)] public bool fConnected;
    [MarshalAs(UnmanagedType.Bool)] public bool fRemembered;
    [MarshalAs(UnmanagedType.Bool)] public bool fAuthenticated;
    public SYSTEMTIME stLastSeen;
    public SYSTEMTIME stLastUsed;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 248)]
    public string szName;
  }

  [StructLayout(LayoutKind.Sequential)]
  public struct BLUETOOTH_DEVICE_SEARCH_PARAMS {
    public uint dwSize;
    [MarshalAs(UnmanagedType.Bool)] public bool fReturnAuthenticated;
    [MarshalAs(UnmanagedType.Bool)] public bool fReturnRemembered;
    [MarshalAs(UnmanagedType.Bool)] public bool fReturnUnknown;
    [MarshalAs(UnmanagedType.Bool)] public bool fReturnConnected;
    [MarshalAs(UnmanagedType.Bool)] public bool fIssueInquiry;
    public byte cTimeoutMultiplier;
    public IntPtr hRadio;
  }

  [StructLayout(LayoutKind.Sequential)]
  public struct BLUETOOTH_FIND_RADIO_PARAMS {
    public uint dwSize;
  }

  [DllImport("BluetoothAPIs.dll", SetLastError = true)]
  static extern IntPtr BluetoothFindFirstRadio(ref BLUETOOTH_FIND_RADIO_PARAMS pbtfrp, out IntPtr phRadio);

  [DllImport("BluetoothAPIs.dll", SetLastError = true)]
  static extern bool BluetoothFindNextRadio(IntPtr hFind, out IntPtr phRadio);

  [DllImport("BluetoothAPIs.dll", SetLastError = true)]
  static extern bool BluetoothFindRadioClose(IntPtr hFind);

  [DllImport("BluetoothAPIs.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern IntPtr BluetoothFindFirstDevice(ref BLUETOOTH_DEVICE_SEARCH_PARAMS pbtsp, ref BLUETOOTH_DEVICE_INFO pbtdi);

  [DllImport("BluetoothAPIs.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern bool BluetoothFindNextDevice(IntPtr hFind, ref BLUETOOTH_DEVICE_INFO pbtdi);

  [DllImport("BluetoothAPIs.dll", SetLastError = true)]
  static extern bool BluetoothFindDeviceClose(IntPtr hFind);

  [DllImport("BluetoothAPIs.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern uint BluetoothEnumerateInstalledServices(IntPtr hRadio, ref BLUETOOTH_DEVICE_INFO pbtdi, ref uint pcServiceInout, [Out] Guid[] pGuidServices);

  [DllImport("BluetoothAPIs.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern uint BluetoothSetServiceState(IntPtr hRadio, ref BLUETOOTH_DEVICE_INFO pbtdi, ref Guid pGuidService, uint dwServiceFlags);

  [DllImport("kernel32.dll", SetLastError = true)]
  static extern bool DeviceIoControl(IntPtr hDevice, uint dwIoControlCode, ref ulong lpInBuffer, int nInBufferSize, IntPtr lpOutBuffer, int nOutBufferSize, out uint lpBytesReturned, IntPtr lpOverlapped);

  [DllImport("kernel32.dll", SetLastError = true)]
  static extern bool CloseHandle(IntPtr hObject);

  public static string FormatMac(ulong addr) {
    return string.Format("{0:X2}:{1:X2}:{2:X2}:{3:X2}:{4:X2}:{5:X2}",
      (addr >> 40) & 0xFF, (addr >> 32) & 0xFF, (addr >> 24) & 0xFF,
      (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF);
  }

  public static string ClassifyDevice(uint cod, string name) {
    uint major = (cod >> 8) & 0x1F;
    bool audioService = (cod & (1u << 21)) != 0 || major == 0x04;
    string n = (name ?? "").ToLowerInvariant();
    if (audioService || n.Contains("airpod") || n.Contains("buds") || n.Contains("headphone") ||
        n.Contains("earbud") || n.Contains("bose") || n.Contains("sony") || n.Contains("xm4") ||
        n.Contains("xm5") || n.Contains("beats") || n.Contains("jbl") || n.Contains("speaker") ||
        n.Contains("headset") || n.Contains("soundcore") || n.Contains("sennheiser")) {
      return "Audio";
    }
    if (major == 0x05 || n.Contains("mouse") || n.Contains("keyboard") ||
        n.Contains("controller") || n.Contains("xbox") || n.Contains("dualshock") || n.Contains("dualsense")) {
      return "Input";
    }
    if (major == 0x02) return "Phone";
    if (major == 0x01) return "PC";
    return "Device";
  }

  public static bool IsRadioPresent() {
    BLUETOOTH_FIND_RADIO_PARAMS rp = new BLUETOOTH_FIND_RADIO_PARAMS();
    rp.dwSize = (uint)Marshal.SizeOf(typeof(BLUETOOTH_FIND_RADIO_PARAMS));
    IntPtr hRadio;
    IntPtr hFind = BluetoothFindFirstRadio(ref rp, out hRadio);
    if (hFind == IntPtr.Zero) return false;
    try {
      if (hRadio != IntPtr.Zero) CloseHandle(hRadio);
    } finally {
      BluetoothFindRadioClose(hFind);
    }
    return true;
  }

  static List<IntPtr> OpenAllRadios() {
    List<IntPtr> list = new List<IntPtr>();
    BLUETOOTH_FIND_RADIO_PARAMS rp = new BLUETOOTH_FIND_RADIO_PARAMS();
    rp.dwSize = (uint)Marshal.SizeOf(typeof(BLUETOOTH_FIND_RADIO_PARAMS));
    IntPtr hRadio;
    IntPtr hFind = BluetoothFindFirstRadio(ref rp, out hRadio);
    if (hFind == IntPtr.Zero) return list;
    try {
      if (hRadio != IntPtr.Zero) list.Add(hRadio);
      while (BluetoothFindNextRadio(hFind, out hRadio)) {
        if (hRadio != IntPtr.Zero) list.Add(hRadio);
      }
    } finally {
      BluetoothFindRadioClose(hFind);
    }
    return list;
  }

  static void CloseAllRadios(List<IntPtr> radios) {
    foreach (IntPtr h in radios) {
      if (h != IntPtr.Zero) {
        try { CloseHandle(h); } catch { }
      }
    }
  }

  public static List<BtDeviceSnapshot> ListPairedDevices() {
    List<BtDeviceSnapshot> result = new List<BtDeviceSnapshot>();
    BLUETOOTH_DEVICE_SEARCH_PARAMS sp = new BLUETOOTH_DEVICE_SEARCH_PARAMS();
    sp.dwSize = (uint)Marshal.SizeOf(typeof(BLUETOOTH_DEVICE_SEARCH_PARAMS));
    sp.fReturnAuthenticated = true;
    sp.fReturnRemembered = true;
    sp.fReturnConnected = true;
    sp.fReturnUnknown = false;
    sp.fIssueInquiry = false;
    sp.cTimeoutMultiplier = 0;
    sp.hRadio = IntPtr.Zero;

    BLUETOOTH_DEVICE_INFO info = new BLUETOOTH_DEVICE_INFO();
    info.dwSize = (uint)Marshal.SizeOf(typeof(BLUETOOTH_DEVICE_INFO));

    IntPtr hFind = BluetoothFindFirstDevice(ref sp, ref info);
    if (hFind == IntPtr.Zero) return result;
    try {
      do {
        if (info.Address == 0) continue;
        Guid[] services = new Guid[16];
        uint count = (uint)services.Length;
        uint err = BluetoothEnumerateInstalledServices(IntPtr.Zero, ref info, ref count, services);
        if (err != 0) count = 0;

        BtDeviceSnapshot snap = new BtDeviceSnapshot();
        snap.Address = info.Address;
        snap.Mac = FormatMac(info.Address);
        snap.Name = string.IsNullOrWhiteSpace(info.szName) ? snap.Mac : info.szName.Trim();
        snap.ClassOfDevice = info.ulClassofDevice;
        snap.Kind = ClassifyDevice(info.ulClassofDevice, snap.Name);
        snap.Connected = info.fConnected;
        snap.Remembered = info.fRemembered;
        snap.InstalledServiceCount = (int)count;
        result.Add(snap);

        info = new BLUETOOTH_DEVICE_INFO();
        info.dwSize = (uint)Marshal.SizeOf(typeof(BLUETOOTH_DEVICE_INFO));
      } while (BluetoothFindNextDevice(hFind, ref info));
    } finally {
      BluetoothFindDeviceClose(hFind);
    }
    return result;
  }

  static Guid[] GetTargetGuids(string kind, BLUETOOTH_DEVICE_INFO info) {
    List<Guid> list = new List<Guid>();
    Guid[] installed = new Guid[16];
    uint count = (uint)installed.Length;
    if (BluetoothEnumerateInstalledServices(IntPtr.Zero, ref info, ref count, installed) == 0) {
      for (int i = 0; i < (int)count && i < installed.Length; i++) {
        if (installed[i] != Guid.Empty && !list.Contains(installed[i])) list.Add(installed[i]);
      }
    }
    Guid[] std = (kind == "Input")
      ? new Guid[] { GuidHid, GuidAudioSink, GuidHandsfree, GuidHeadset }
      : new Guid[] { GuidAudioSink, GuidHandsfree, GuidHeadset, GuidAvRemoteTarget, GuidAvRemote, GuidGenericAudio };
    for (int i = 0; i < std.Length; i++) {
      if (!list.Contains(std[i])) list.Add(std[i]);
    }
    return list.ToArray();
  }

  // Blocks auto-connect profiles AND drops any active ACL link immediately.
  public static string BlockAndDisconnect(ulong address, string kind) {
    BLUETOOTH_DEVICE_INFO info = new BLUETOOTH_DEVICE_INFO();
    info.dwSize = (uint)Marshal.SizeOf(typeof(BLUETOOTH_DEVICE_INFO));
    info.Address = address;

    Guid[] targets = GetTargetGuids(kind, info);
    for (int i = 0; i < targets.Length; i++) {
      Guid g = targets[i];
      try { BluetoothSetServiceState(IntPtr.Zero, ref info, ref g, BLUETOOTH_SERVICE_DISABLE); } catch { }
    }

    List<IntPtr> radios = OpenAllRadios();
    try {
      foreach (IntPtr h in radios) {
        ulong addr = address;
        uint bytes;
        try { DeviceIoControl(h, IOCTL_BTH_DISCONNECT_DEVICE, ref addr, 8, IntPtr.Zero, 0, out bytes, IntPtr.Zero); } catch { }
      }
    } finally {
      CloseAllRadios(radios);
    }
    return "ok";
  }

  // Re-enables profiles so Windows connects now (or allows normal auto-connect).
  public static string EnableAndConnect(ulong address, string kind) {
    BLUETOOTH_DEVICE_INFO info = new BLUETOOTH_DEVICE_INFO();
    info.dwSize = (uint)Marshal.SizeOf(typeof(BLUETOOTH_DEVICE_INFO));
    info.Address = address;

    Guid[] targets = (kind == "Input")
      ? new Guid[] { GuidHid }
      : new Guid[] { GuidAudioSink, GuidHandsfree, GuidHeadset, GuidAvRemoteTarget, GuidAvRemote };
    for (int i = 0; i < targets.Length; i++) {
      Guid g = targets[i];
      try { BluetoothSetServiceState(IntPtr.Zero, ref info, ref g, BLUETOOTH_SERVICE_ENABLE); } catch { }
    }
    return "ok";
  }
}

public static class PowerEvents {
  static int _resumes = 0;
  static int _suspends = 0;
  static bool _hooked = false;

  public static void Start() {
    if (_hooked) return;
    _hooked = true;
    try { Microsoft.Win32.SystemEvents.PowerModeChanged += OnPower; } catch { }
    try { Microsoft.Win32.SystemEvents.SessionSwitch += OnSession; } catch { }
  }

  public static void Stop() {
    if (!_hooked) return;
    _hooked = false;
    try { Microsoft.Win32.SystemEvents.PowerModeChanged -= OnPower; } catch { }
    try { Microsoft.Win32.SystemEvents.SessionSwitch -= OnSession; } catch { }
  }

  static void OnPower(object s, Microsoft.Win32.PowerModeChangedEventArgs e) {
    if (e.Mode == Microsoft.Win32.PowerModes.Resume) Interlocked.Increment(ref _resumes);
    if (e.Mode == Microsoft.Win32.PowerModes.Suspend) Interlocked.Increment(ref _suspends);
  }

  static void OnSession(object s, Microsoft.Win32.SessionSwitchEventArgs e) {
    Interlocked.Increment(ref _resumes);
  }

  public static int ConsumeResumes() { return Interlocked.Exchange(ref _resumes, 0); }
  public static int ConsumeSuspends() { return Interlocked.Exchange(ref _suspends, 0); }
}

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
  public int Radius = 10;
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
      g.DrawPath(pen, p);
    }
    base.OnPaint(e);
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
    Height = 26;
    Cursor = Cursors.Hand;
  }
  protected override void OnClick(EventArgs e) { base.OnClick(e); Checked = !Checked; }
  protected override void OnMouseEnter(EventArgs e) { base.OnMouseEnter(e); _hover = true;  Invalidate(); }
  protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); _hover = false; Invalidate(); }

  protected override void OnPaint(PaintEventArgs e) {
    Graphics g = e.Graphics;
    g.SmoothingMode = SmoothingMode.AntiAlias;
    g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
    int h = 20, w = 38;
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
    using (SolidBrush sh = new SolidBrush(Color.FromArgb(60, 0, 0, 0)))
      g.FillEllipse(sh, kx + 1, ty + 4, kd, kd);
    using (SolidBrush b = new SolidBrush(_on ? Color.White : Color.FromArgb(188, 194, 212)))
      g.FillEllipse(b, kx, ty + 3, kd, kd);

    Rectangle tr = new Rectangle(w + 10, 0, Width - w - 10, Height);
    TextRenderer.DrawText(g, Text, Font, tr, Theme.Text,
      TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
  }
}

public class FlatBtn : Control {
  bool _hover = false, _down = false;
  public bool Primary = false;
  public bool Danger = false;
  public FlatBtn() {
    SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint |
             ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
             ControlStyles.SupportsTransparentBackColor, true);
    BackColor = Color.Transparent;
    Height = 34;
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
    using (GraphicsPath p = Theme.Round(r, 8)) {
      if (Primary) {
        Color a = Theme.Accent2, b2 = Theme.Accent;
        if (_down)       { a = Color.FromArgb(96, 74, 214); b2 = Color.FromArgb(0, 132, 210); }
        else if (_hover) { a = Color.FromArgb(136, 108, 255); b2 = Color.FromArgb(28, 178, 255); }
        using (LinearGradientBrush b = new LinearGradientBrush(new Rectangle(0, 0, Math.Max(Width, 1), Height), a, b2, LinearGradientMode.Horizontal))
          g.FillPath(b, p);
      } else {
        Color c = Theme.Panel2;
        if (_down)       c = Color.FromArgb(28, 30, 40);
        else if (_hover) c = Danger ? Color.FromArgb(88, 38, 44) : Theme.Line;
        using (SolidBrush b = new SolidBrush(c)) g.FillPath(b, p);
        using (Pen pen = new Pen(Danger && _hover ? Theme.Bad : Theme.Line)) g.DrawPath(pen, p);
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
