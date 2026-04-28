using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Management;
using HidLibrary;
using System.Linq;
using System.Runtime.InteropServices;
using System.IO;
using System.Drawing;
using System.Net.Sockets;

namespace DT_Controller
{
    public partial class Form1 : Form
    {
        // ����ʾ�˹��������豸
        private const int FilterVendorId = 0x0483;
        private const int FilterProductId = 0xA7D1;

        // WM_DEVICECHANGE ����Ϣ���¼�
        private const int WM_DEVICECHANGE = 0x0219;
        private const int DBT_DEVICEARRIVAL = 0x8000;
        private const int DBT_DEVICEREMOVECOMPLETE = 0x8004;
        private const int WM_KEYDOWN = 0x0100;
        private const int WM_KEYUP = 0x0101;
        private const int WM_SYSKEYDOWN = 0x0104;
        private const int WM_SYSKEYUP = 0x0105;

        // ע���豸֪ͨ�ó���
        private const int DBT_DEVTYP_DEVICEINTERFACE = 0x00000005;
        private const uint DEVICE_NOTIFY_WINDOW_HANDLE = 0x00000000;

        // USB �豸�ӿ� GUID��USB device interface��
        // ���ڽ��ղ��֪ͨ��{A5DCBF10-6530-11D2-901F-00C04FB951ED}
        private static readonly Guid GUID_DEVINTERFACE_USB_DEVICE = new Guid("A5DCBF10-6530-11D2-901F-00C04FB951ED");

        // ����ȥ������β��֪ͨ�� UI ��ʱ��
        private readonly System.Windows.Forms.Timer deviceChangeTimer;

        // ע����
        private IntPtr deviceNotificationHandle = IntPtr.Zero;

        // �־û������к�->����ӳ��
        private readonly Dictionary<string, string> nameMappings = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        private readonly string mappingsFilePath;

        // �Ҽ��˵� (Device List)
        private readonly ContextMenuStrip deviceContextMenu;

        // Module List �Ҽ��˵�
        private readonly ContextMenuStrip moduleContextMenu;

        // ��ǰ���ڶ�ȡ HID ���ĵ��豸����һ��ʵ����
        private HidDevice currentHidDevice;
        private readonly object hidLock = new object();

        // �������ݼ���/ʱ���
        private DateTime lastReceiveTime = DateTime.MinValue;

        // �����ֶΣ��������кţ�seq�������������ֶ�����
        private int seqCounter = 0;

        // �����ֶΣ���ֹ�ظ����� Module ��ѯ��ÿ������ֻ����һ�Σ�
        private bool moduleQuerySent = false;

        // �洢��ǰ�豸�� module �б�
        private readonly List<ModuleInfo> currentModules = new List<ModuleInfo>();

        // Remaining step cache (Device -> PC report CMD_MOTION 0x10)
        private uint remainingStepX = 0;
        private uint remainingStepY = 0;

        private int lastPosX = 0;
        private int lastPosY = 0;
        private bool hasLastPos = false;

        private int _pendingXCurrentUi = -1;
        private DateTime _pendingXCurrentUntil = DateTime.MinValue;
        private int _pendingXCurrentModuleIndex = -1;
        private int _pendingYCurrentUi = -1;
        private DateTime _pendingYCurrentUntil = DateTime.MinValue;
        private int _pendingYCurrentModuleIndex = -1;
        private int _pendingHoldCurrentUi = -1;
        private DateTime _pendingHoldCurrentUntil = DateTime.MinValue;
        private int _pendingHoldCurrentModuleIndex = -1;
        private int _confirmedXCurrentUi = -1;
        private DateTime _confirmedXCurrentHoldUntil = DateTime.MinValue;
        private int _confirmedXCurrentModuleIndex = -1;
        private int _confirmedYCurrentUi = -1;
        private DateTime _confirmedYCurrentHoldUntil = DateTime.MinValue;
        private int _confirmedYCurrentModuleIndex = -1;
        private int _confirmedHoldCurrentUi = -1;
        private DateTime _confirmedHoldCurrentHoldUntil = DateTime.MinValue;
        private int _confirmedHoldCurrentModuleIndex = -1;
        private int _pendingStepSpeedUi = -1;
        private DateTime _pendingStepSpeedUntil = DateTime.MinValue;
        private int _pendingStepSpeedModuleIndex = -1;
        private int _confirmedStepSpeedUi = -1;
        private DateTime _confirmedStepSpeedHoldUntil = DateTime.MinValue;
        private int _confirmedStepSpeedModuleIndex = -1;
        private readonly Dictionary<uint, int> _manualStepSpeedByNodeId = new Dictionary<uint, int>();

        private const byte CMD_PARAM_SET = 0x20;

        // Cache last selected module NodeID (parsed from ModuleInfo bytes)
        private uint _currentNodeId = 0;

        internal enum ParamId : byte
        {
            X_Cur = 0x01,
            Y_Cur = 0x02,
            Hold = 0x03,
            Speed = 0x04
        }

        private ITransport _transport;
        private TcpTransport _tcpTransport;
        private bool _usingTcp = false;

        // ETH device IP (for Device_Info display)
        private string _pendingEthIp;

        // UDP discovery (listen for MCU broadcast announcements)
        private UdpClient _discoveryClient;
        private System.Windows.Forms.Timer _discoveryCleanupTimer;
        private const int UDP_DISCOVERY_PORT = 40001;
        private readonly Dictionary<Button, Image> _pressedButtonImages = new Dictionary<Button, Image>();
        private readonly Dictionary<Button, Image> _normalButtonImages = new Dictionary<Button, Image>();
        private readonly Dictionary<Button, int> _buttonPressRefCount = new Dictionary<Button, int>();
        private readonly HashSet<Keys> _heldDirectionKeys = new HashSet<Keys>();

        private sealed class HidTransport : ITransport
        {
            private readonly Func<HidDevice> _getDevice;
            private readonly object _lock;

            public HidTransport(Func<HidDevice> getDevice, object hidLock)
            {
                _getDevice = getDevice;
                _lock = hidLock;
            }

            public void Write(byte[] frame)
            {
                if (frame == null || frame.Length == 0)
                    return;

                lock (_lock)
                {
                    var dev = _getDevice?.Invoke();
                    if (dev == null || !dev.IsOpen || !dev.IsConnected)
                        return;

                    dev.Write(frame);
                }
            }

        }

        public Form1()
        {
            InitializeComponent();

            ConfigureTransparentImageButton(button_up);
            ConfigureTransparentImageButton(button_down);
            ConfigureTransparentImageButton(button_left);
            ConfigureTransparentImageButton(button_right);
            ConfigureTransparentImageButton(button_stop);

            ConfigurePressedImageButton(button_up, "ico_direction_up_grey.png");
            ConfigurePressedImageButton(button_down, "ico_direction_down_grey.png");
            ConfigurePressedImageButton(button_left, "ico_direction_left_grey.png");
            ConfigurePressedImageButton(button_right, "ico_direction_right_grey.png");
            ConfigurePressedImageButton(button_stop, "stop_76x76_grey.png");

            // Enable key preview to handle step hotkeys
            this.KeyPreview = true;
            this.KeyDown += Form1_KeyDown_DirectionPressedState;
            this.KeyUp += Form1_KeyUp_StepHotkeys;

            checkBoxXY.Checked = true;
            checkBoxX.Checked = false;
            checkBoxY.Checked = true;
            checkBoxXY.CheckedChanged += AxisMappingCheckBox_CheckedChanged;
            checkBoxX.CheckedChanged += AxisMappingCheckBox_CheckedChanged;
            checkBoxY.CheckedChanged += AxisMappingCheckBox_CheckedChanged;

            _transport = new HidTransport(() => currentHidDevice, hidLock);

            // Designer ��Ϊ MainDevice �� SelectedIndexChanged������ֻ�� Load
            this.Load += Form1_Load;

            // ���� SubDevice �������ʾ Module Info
            SubDevice.SelectedIndexChanged += SubDevice_SelectedIndexChanged;

            // Bind param +/- buttons
            try
            {
                button_xCurrentPlus.Click += button_xCurrentPlus_Click;
                button_xCurrentMinus.Click += button_xCurrentMinus_Click;
                button_yCurrentPlus.Click += button_yCurrentPlus_Click;
                button_yCurrentMinus.Click += button_yCurrentMinus_Click;
                button_holdCurrentPlus.Click += button_holdCurrentPlus_Click;
                button_holdCurrentMinus.Click += button_holdCurrentMinus_Click;
                button_stepSpeedPlus.Click += button_stepSpeedPlus_Click;
                button_stepSpeedMinus.Click += button_stepSpeedMinus_Click;
            }
            catch
            {
                // ignore
            }

            // ��ʼ��ȥ������ʱ����500ms��
            deviceChangeTimer = new System.Windows.Forms.Timer();
            deviceChangeTimer.Interval = 500;
            deviceChangeTimer.Tick += DeviceChangeTimer_Tick;

            // �ڴ���ر�ʱ�ͷż�ʱ����ע���豸֪ͨ��ͬʱֹͣ HID ��ȡ
            this.FormClosed += (s, e) =>
            {
                deviceChangeTimer?.Dispose();
                StopListeningCurrentDevice();
                UnregisterDeviceNotificationSafe();
                DisposePressedButtonImages();
            };
            this.Deactivate += (s, e) => ReleaseAllPressedDirectionButtons();

            // ӳ���ļ�λ�ã��û�Ӧ������Ŀ¼��
            mappingsFilePath = Path.Combine(Application.UserAppDataPath, "device_names.map");

            // �����Ҽ��˵�
            deviceContextMenu = new ContextMenuStrip();
            var miChange = new ToolStripMenuItem("Change Name", null, ChangeName_Click);
            var miUpgrade = new ToolStripMenuItem("Upgrade", null, Upgrade_Click) { Enabled = false }; // ��ʱ��ɫ������
            deviceContextMenu.Items.AddRange(new ToolStripItem[] { miChange, new ToolStripSeparator(), miUpgrade });

            // �� ListBox �Ҽ��¼�
            MainDevice.MouseDown += MainDevice_MouseDown;

            // �����Ҽ��˵���Module List��
            moduleContextMenu = new ContextMenuStrip();
            var miModChange = new ToolStripMenuItem("Change name", null, ModuleChangeName_Click);
            var miModReset = new ToolStripMenuItem("Reset name", null, ModuleResetName_Click);
            var miModFwUpgrade = new ToolStripMenuItem("FW upgrade", null, ModuleFwUpgrade_Click) { Enabled = false };
            moduleContextMenu.Items.AddRange(new ToolStripItem[] { miModChange, miModReset, new ToolStripSeparator(), miModFwUpgrade });

            // �� Module List �Ҽ��¼�
            SubDevice.MouseDown += SubDevice_MouseDown;

            // ��������ӳ�䣨������ڣ�
            LoadNameMappingsSafe();
        }

        // ListBox �Ҽ���ѡ�в���ʾ�����Ĳ˵�
        private void MainDevice_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                int idx = MainDevice.IndexFromPoint(e.Location);
                if (idx >= 0 && idx < MainDevice.Items.Count)
                {
                    MainDevice.SelectedIndex = idx;
                    var obj = MainDevice.Items[idx];

                    if (obj is EthDeviceItem)
                    {
                        deviceContextMenu.Items[0].Enabled = true;
                        deviceContextMenu.Items[2].Enabled = true; // Upgrade enabled for ETH
                    }
                    else if (obj is HidDeviceItem hid)
                    {
                        bool hasSerial = !string.IsNullOrWhiteSpace(hid.Serial);
                        deviceContextMenu.Items[0].Enabled = hasSerial;
                        deviceContextMenu.Items[2].Enabled = false; // Upgrade
                    }

                    deviceContextMenu.Show(MainDevice, e.Location);
                }
            }
        }

        // Change Name
        private void ChangeName_Click(object sender, EventArgs e)
        {
            int idx = MainDevice.SelectedIndex;
            if (idx < 0) return;

            var obj = MainDevice.Items[idx];

            if (obj is EthDeviceItem ethItem)
            {
                using (var prompt = new SingleLinePrompt("Change Name", "Enter new name (max 16 chars, empty to reset):", ethItem.Hostname ?? string.Empty, 16))
                {
                    if (prompt.ShowDialog(this) != DialogResult.OK)
                        return;
                    var newName = (prompt.Value ?? string.Empty).Trim();

                    // Send to MCU (empty = clear custom name, revert to default)
                    SendSetDeviceName(newName);

                    // Update display: empty means default will come from next UDP broadcast
                    if (!string.IsNullOrEmpty(newName))
                        ethItem.Hostname = newName;
                    MainDevice.Items[idx] = ethItem;
                }
            }
            else if (obj is HidDeviceItem item && !string.IsNullOrWhiteSpace(item.Serial))
            {
                using (var prompt = new SingleLinePrompt("Change Name", "Enter new name:", nameMappings.ContainsKey(item.Serial) ? nameMappings[item.Serial] : item.Serial))
                {
                    if (prompt.ShowDialog(this) == DialogResult.OK)
                    {
                        var newName = prompt.Value?.Trim();
                        if (!string.IsNullOrWhiteSpace(newName))
                        {
                            nameMappings[item.Serial] = newName;
                            item.DisplayName = newName;
                            if (idx >= 0)
                                MainDevice.Items[idx] = item;
                            SaveNameMappingsSafe();
                            RefreshDisplayNames();
                        }
                    }
                }
            }
        }

        private void SendSetDeviceName(string name)
        {
            var nameBytes = new byte[16];
            if (!string.IsNullOrEmpty(name))
            {
                var ascii = Encoding.ASCII.GetBytes(name);
                Array.Copy(ascii, 0, nameBytes, 0, Math.Min(16, ascii.Length));
            }

            var cmd = new byte[64];
            cmd[0] = 0xA5;
            cmd[1] = 0x01; // CMD_INFO
            cmd[2] = 0x02; // PC -> MCU

            ushort seq = GetNextSeq();
            cmd[3] = (byte)(seq & 0xFF);
            cmd[4] = (byte)((seq >> 8) & 0xFF);

            cmd[5] = 17;   // payload length: subcmd(1) + name(16)
            cmd[6] = 0x06; // SUBCMD_SET_DEVICE_NAME

            Array.Copy(nameBytes, 0, cmd, 7, 16);

            SendToHID(cmd);
        }

        // Upgrade �����Ŀǰ�����ã�
        private async void Upgrade_Click(object sender, EventArgs e)
        {
            var eth = MainDevice.SelectedItem as EthDeviceItem;
            if (eth == null)
            {
                MessageBox.Show(this, "Please select an ETH device.", "Upgrade",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            using (var dlg = new System.Windows.Forms.OpenFileDialog())
            {
                dlg.Filter = "ALM firmware (*.cic)|*.cic|All files (*.*)|*.*";
                dlg.Title  = "Select firmware file";
                if (dlg.ShowDialog(this) != DialogResult.OK)
                    return;

                byte[] almData;
                try { almData = File.ReadAllBytes(dlg.FileName); }
                catch (Exception ex)
                {
                    MessageBox.Show(this, "Cannot read file: " + ex.Message,
                        "Upgrade", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                // disable the menu item during transfer
                var miUp = deviceContextMenu.Items[2] as ToolStripMenuItem;
                if (miUp != null) miUp.Enabled = false;
                var origTitle = this.Text;

                try
                {
                    // Release the existing TCP control connection so the device
                    // can accept the upgrade connection (single-client server).
                    DisconnectTcp();
                    await Task.Delay(300);

                    await UpgradeSendFirmwareAsync(eth, almData,
                        pct => { try { this.Text = $"Upgrading... {pct}%"; } catch { } });

                    MessageBox.Show(this,
                        "Firmware sent. Device is rebooting to apply the update.",
                        "Upgrade", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, "Upgrade failed: " + ex.Message,
                        "Upgrade Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                finally
                {
                    this.Text = origTitle;
                    if (miUp != null) miUp.Enabled = true;
                }
            }
        }

        // Module List �Ҽ���ѡ�в���ʾ�����Ĳ˵�
        private void SubDevice_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Right)
                return;

            int idx;
            try { idx = SubDevice.IndexFromPoint(e.Location); }
            catch { return; }

            if (idx < 0 || idx >= SubDevice.Items.Count)
                return;

            SubDevice.SelectedIndex = idx;

            var mi = SubDevice.Items[idx] as ModuleInfo;
            bool hasModule = mi != null;

            moduleContextMenu.Items[0].Enabled = hasModule; // Change name
            moduleContextMenu.Items[1].Enabled = hasModule; // Reset name
            // FW upgrade: needs both an ETH device (route) and a module (target).
            moduleContextMenu.Items[3].Enabled =
                hasModule && (MainDevice.SelectedItem is EthDeviceItem);

            moduleContextMenu.Show(SubDevice, e.Location);
        }

        private void ModuleChangeName_Click(object sender, EventArgs e)
        {
            if (!(SubDevice.SelectedItem is ModuleInfo mi))
                return;

            using (var prompt = new SingleLinePrompt("Change name", "Enter new module name (max 8 chars):", mi.CustomName ?? mi.Name ?? string.Empty, 8))
            {
                if (prompt.ShowDialog(this) != DialogResult.OK)
                    return;

                var newName = (prompt.Value ?? string.Empty).Trim();
                if (string.IsNullOrWhiteSpace(newName))
                    return;

                // Send Set Module Name to device (ASCII, 8 bytes)
                SendSetModuleName(mi, newName);
            }
        }

        private void ModuleResetName_Click(object sender, EventArgs e)
        {
            if (!(SubDevice.SelectedItem is ModuleInfo mi))
                return;

            // Reset name: send 0-filled name bytes
            SendSetModuleName(mi, null);
        }

        private async void ModuleFwUpgrade_Click(object sender, EventArgs e)
        {
            var mi  = SubDevice.SelectedItem as ModuleInfo;
            var eth = MainDevice.SelectedItem as EthDeviceItem;
            if (mi == null || eth == null)
            {
                MessageBox.Show(this, "Select an ETH device + a Module first.",
                    "Module FW upgrade", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            using (var dlg = new System.Windows.Forms.OpenFileDialog())
            {
                dlg.Filter = "Motor firmware (*.mot)|*.mot|All files (*.*)|*.*";
                dlg.Title  = $"Select firmware for module {mi.Name} (NodeID 0x{mi.NodeId:X8})";
                if (dlg.ShowDialog(this) != DialogResult.OK)
                    return;

                byte[] motData;
                try { motData = File.ReadAllBytes(dlg.FileName); }
                catch (Exception ex)
                {
                    MessageBox.Show(this, "Cannot read file: " + ex.Message,
                        "Module FW upgrade", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                if (motData.Length < 32)
                {
                    MessageBox.Show(this, "File too small to be a valid .mot.",
                        "Module FW upgrade", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                var miUp = moduleContextMenu.Items[3] as ToolStripMenuItem;
                if (miUp != null) miUp.Enabled = false;
                var origTitle = this.Text;

                try
                {
                    DisconnectTcp();
                    await Task.Delay(300);

                    await ModuleUpgradeSendFirmwareAsync(eth, mi.NodeId, motData,
                        pct => { try { this.Text = $"Upgrading module... {pct}%"; } catch { } });

                    MessageBox.Show(this,
                        "Firmware staged on device. Module will reboot to apply the update.",
                        "Module FW upgrade", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, "Upgrade failed: " + ex.Message,
                        "Module FW upgrade error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                finally
                {
                    this.Text = origTitle;
                    if (miUp != null) miUp.Enabled = true;
                }
            }
        }

        private void SendSetModuleName(ModuleInfo mi, string name)
        {
            if (mi == null)
                return;

            // fixed 8 bytes ASCII (no guarantee of \0)
            var nameBytes = new byte[8];
            if (!string.IsNullOrEmpty(name))
            {
                var ascii = Encoding.ASCII.GetBytes(name);
                Array.Copy(ascii, 0, nameBytes, 0, Math.Min(8, ascii.Length));
            }

            // Build command:
            // 0 A5
            // 1 CMD_INFO (0x01)
            // 2 DIR PC->MCU (0x02)
            // 3 SEQ_L
            // 4 SEQ_H
            // 5 LENGTH
            // 6 SUBCMD_SET_MODULE_NAME (0x04)
            // 7 Module Index
            // 8..11 NodeID (LSB first)
            // 12..19 Name (8 bytes)
            var cmd = new byte[64];
            cmd[0] = 0xA5;
            cmd[1] = 0x01; // CMD_INFO / CMD_DEVICE_INFO
            cmd[2] = 0x02; // PC -> MCU

            ushort seq = GetNextSeq();
            cmd[3] = (byte)(seq & 0xFF);
            cmd[4] = (byte)((seq >> 8) & 0xFF);

            cmd[5] = 14; // payload length: subcmd(1)+idx(1)+node(4)+name(8)
            cmd[6] = 0x04; // SUBCMD_SET_MODULE_NAME
            cmd[7] = (byte)mi.Index;

            uint nodeId = 0;
            uint.TryParse(mi.SN, out nodeId);
            cmd[8] = (byte)(nodeId & 0xFF);
            cmd[9] = (byte)((nodeId >> 8) & 0xFF);
            cmd[10] = (byte)((nodeId >> 16) & 0xFF);
            cmd[11] = (byte)((nodeId >> 24) & 0xFF);

            Array.Copy(nameBytes, 0, cmd, 12, 8);

            SendToHID(cmd);

            // After 1500ms, refresh only this module by querying Module Info (subcmd 0x03)
            Task.Run(async () =>
            {
                try
                {
                    await Task.Delay(1500).ConfigureAwait(false);
                }
                catch { }

                try
                {
                    var req = BuildDeviceInfoQueryBuffer(GetNextSeq(), 0x03, new byte[] { (byte)mi.Index });
                    SendToHID(req);
                }
                catch
                {
                    // ignore
                }
            });
        }

        private void DeviceChangeTimer_Tick(object sender, EventArgs e)
        {
            deviceChangeTimer.Stop();
            // �� UI �̵߳����첽ˢ��
            PopulateHidDevicesAsync();
        }

        // ��д WndProc �����豸�����Ϣ�����£�����ʱ��������ö�٣��Ƴ�ʱ���������Ƴ��߼���
        protected override void WndProc(ref Message m)
        {
            if (m.Msg == WM_DEVICECHANGE)
            {
                int wParam = (int)m.WParam;
                if (wParam == DBT_DEVICEARRIVAL)
                {
                    // �豸���룺��������ȥ����ˢ�£�����������������Ӧ�� WMI/����ע���ͺ�
                    RequestRefreshDevices();
                    StartArrivalRetry();
                }
                else if (wParam == DBT_DEVICEREMOVECOMPLETE)
                {
                    try
                    {
                        var selItem = MainDevice.SelectedItem as HidDeviceItem;
                        string selPath = selItem?.DevicePath;

                        // ֹͣ��ǰ HID �������ͷ���Դ
                        StopListeningCurrentDevice();

                        // �������б����Ƴ��� selPath ƥ�����
                        if (!string.IsNullOrWhiteSpace(selPath))
                        {
                            for (int i = MainDevice.Items.Count - 1; i >= 0; i--)
                            {
                                if (MainDevice.Items[i] is HidDeviceItem it &&
                                    !string.IsNullOrWhiteSpace(it.DevicePath) &&
                                    string.Equals(it.DevicePath, selPath, StringComparison.OrdinalIgnoreCase))
                                {
                                    MainDevice.Items.RemoveAt(i);
                                }
                            }
                        }

                        // ���ѡ���ˢ����� UI ��ʾ
                        MainDevice.SelectedIndex = -1;
                        UpdateDeviceInfo(null);

                        // ������ͬʱ��� SubDevice �б��� Module_Info ��ʾ�������ڴ���ģ���б�
                        try
                        {
                            InvokeIfRequired(() =>
                            {
                                try
                                {
                                    currentModules.Clear();
                                    SubDevice.Items.Clear();
                                    Module_Info_richTextBox.Clear();
                                }
                                catch
                                {
                                    // ���� UI ��������
                                }
                            });
                        }
                        catch
                        {
                            // �����쳣
                        }
                    }
                    catch
                    {
                        // ����ֹͣ/�Ƴ�ʧ��
                    }

                    RequestRefreshDevices();
                }
            }

            base.WndProc(ref m);
        }

        // ����ˢ�£�����ȥ������ʱ����
        private void RequestRefreshDevices()
        {
            try
            {
                deviceChangeTimer.Stop();
                deviceChangeTimer.Start();
            }
            catch
            {
                // ���Լ�ʱ���쳣
            }
        }

        private void label3_Click(object sender, EventArgs e)
        {
        }

        // �����˶���ť��ʹ��״̬
        private void UpdateMotionButtonsEnabledState()
        {
            InvokeIfRequired(() =>
            {
                try
                {
                    // keep motion buttons enabled regardless of remaining-step feedback
                    button_left.Enabled = true;
                    button_right.Enabled = true;
                    button_up.Enabled = true;
                    button_down.Enabled = true;
                }
                catch
                {
                    // ignore UI errors
                }
            });
        }

        private void AxisMappingCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            RefreshPositionLabel();
            RefreshRemainLabel();
        }

        private void RefreshRemainLabel()
        {
            InvokeIfRequired(() =>
            {
                uint x = remainingStepX;
                uint y = remainingStepY;

                if (checkBoxXY != null && checkBoxXY.Checked)
                {
                    var t = x;
                    x = y;
                    y = t;
                }

                var xStr = x == 0 ? "0" : "-" + x;
                var yStr = y == 0 ? "0" : "-" + y;

                try { remain.Text = $"X: {xStr} / Y: {yStr}"; } catch { }
            });
        }

        private void UpdatePositionLabelFromRaw(int x, int y)
        {
            lastPosX = x;
            lastPosY = y;
            hasLastPos = true;
            RefreshPositionLabel();
        }

        private void RefreshPositionLabel()
        {
            InvokeIfRequired(() =>
            {
                if (!hasLastPos)
                    return;

                int x = lastPosX;
                int y = lastPosY;

                if (checkBoxXY != null && checkBoxXY.Checked)
                {
                    int t = x;
                    x = y;
                    y = t;
                }

                try { label_pos.Text = $"Pos  X:{x}  Y:{y}"; } catch { }
            });
        }

        // ѡ���豸ʱ��ʼ/�л�����
        private void MainDevice_SelectedIndexChanged(object sender, EventArgs e)
        {
            // ��ֹ֮ͣǰ�ļ���
            StopListeningCurrentDevice();
            _pendingEthIp = null;

            // reset remaining steps & buttons for new selection
            remainingStepX = 0;
            remainingStepY = 0;
            hasLastPos = false;
            try { remain.Text = "X: 0 / Y: 0"; } catch { }
            try { label_pos.Text = "Pos  X:0000  Y:0000"; } catch { }
            UpdateMotionButtonsEnabledState();

            // Disconnect previous TCP if switching away
            if (_usingTcp)
                DisconnectTcp();

            if (MainDevice.SelectedItem is EthDeviceItem ethItem)
            {
                richTextBox1.Clear();
                richTextBox1.AppendText($"ETH Device: {ethItem.Hostname}\n");
                richTextBox1.AppendText($"IP: {ethItem.IP}  Port: {ethItem.TcpPort}\n");
                richTextBox1.AppendText($"MAC: {BitConverter.ToString(ethItem.MAC)}\n");
                richTextBox1.AppendText($"FW: {ethItem.FW}\n");

                InvokeIfRequired(() =>
                {
                    _manualStepSpeedByNodeId.Clear();
                    currentModules.Clear();
                    SubDevice.Items.Clear();
                    Module_Info_richTextBox.Clear();
                });

                // Show IP in Device_Info (FW/Date will be filled after TCP query)
                _pendingEthIp = ethItem.IP;
                UpdateDeviceInfo(null);

                ConnectTcp(ethItem.IP, ethItem.TcpPort);
                return;
            }

            if (MainDevice.SelectedItem is HidDeviceItem item)
            {
                richTextBox1.Clear();
                richTextBox1.AppendText($"Name: {item.DisplayName}\n");
                richTextBox1.AppendText($"DevicePath: {item.DevicePath}\n");
                richTextBox1.AppendText($"VID: 0x{item.VendorId:X4}  PID: 0x{item.ProductId:X4}\n");
                richTextBox1.AppendText($"Manufacturer: {item.Manufacturer}\n");
                richTextBox1.AppendText($"Product: {item.Product}\n");

                // ���� Device_Info����ʾ SN / FW / Date��FW �� Date Ŀǰռλ��
                UpdateDeviceInfo(item);

                // ��� SubDevice & Module_Info
                InvokeIfRequired(() =>
                {
                    _manualStepSpeedByNodeId.Clear();
                    currentModules.Clear();
                    SubDevice.Items.Clear();
                    Module_Info_richTextBox.Clear();
                });

                // �����Ը��豸�� HID ���ݼ������첽��
                StartListeningToDevice(item);
            }
            else
            {
                UpdateDeviceInfo(null);
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            // ע���豸֪ͨ��ע�� USB device interface������ Load ʱע����ȷ������ handle �Ѵ���
            RegisterForDeviceNotifications();
            PopulateHidDevicesAsync();
            StartEthDiscovery();
        }

        private async void PopulateHidDevicesAsync()
        {
            try
            {
                MainDevice.Items.Clear();
                richTextBox1.Clear();
                richTextBox1.AppendText($"Enumerating HID devices... (filter: VID=0x{FilterVendorId:X4} PID=0x{FilterProductId:X4})" + Environment.NewLine);

                var items = await Task.Run(() => EnumerateHidViaWmiAndHidLibMergedPreferUsbSerial());

                InvokeIfRequired(() =>
                {
                    foreach (var it in items)
                    {
                        // ��������û����õ���ʾ���򸲸� DisplayName
                        if (!string.IsNullOrWhiteSpace(it.Serial) && nameMappings.TryGetValue(it.Serial, out var mapped))
                        {
                            it.DisplayName = mapped;
                        }
                        MainDevice.Items.Add(it);
                    }

                    richTextBox1.Clear();
                    richTextBox1.AppendText($"Found {MainDevice.Items.Count} HID device(s) (filtered).\n");

                    // ������Ŀ��Ĭ����ʾ��һ�������к�
                    var firstSerial = items.FirstOrDefault()?.Serial;
                    // SN_Lab removed; do not overwrite remain label
                });
            }
            catch (Exception ex)
            {
                InvokeIfRequired(() =>
                {
                    richTextBox1.Clear();
                    richTextBox1.AppendText("Enumerate HID devices failed:\n" + ex.Message);
                });
            }
        }

        // ������ָ�� HidDeviceItem �ļ������᳢��ƥ�䲢��һ�� HidLibrary.HidDevice��
        private void StartListeningToDevice(HidDeviceItem item)
        {
            if (item == null || item.VendorId == 0 || item.ProductId == 0)
                return;

            Task.Run(() =>
            {
                try
                {
                    var devices = HidDevices.Enumerate(item.VendorId, item.ProductId).ToArray();
                    HidDevice chosen = null;

                    foreach (var dev in devices)
                    {
                        try
                        {
                            if (dev == null) continue;
                            dev.OpenDevice();
                            var dp = dev.DevicePath ?? string.Empty;
                            if (!string.IsNullOrWhiteSpace(item.Serial))
                            {
                                if (dp.IndexOf(item.Serial, StringComparison.OrdinalIgnoreCase) >= 0)
                                {
                                    chosen = dev;
                                    break;
                                }
                            }

                            if (chosen == null)
                            {
                                chosen = dev;
                                break;
                            }
                        }
                        catch
                        {
                            try { dev.CloseDevice(); } catch { }
                            try { dev.Dispose(); } catch { }
                        }
                    }

                    if (chosen == null)
                    {
                        foreach (var d in devices) { try { d.CloseDevice(); d.Dispose(); } catch { } }
                        return;
                    }

                    lock (hidLock)
                    {
                        StopListeningCurrentDevice();

                        currentHidDevice = chosen;
                        moduleQuerySent = false; // ���豸���������� Module ��ѯ
                        if (!currentHidDevice.IsOpen)
                            currentHidDevice.OpenDevice();

                        // ���� Device Info ��ѯ����������������дʧ�ܣ�
                        try
                        {
                            ushort seq = (ushort)(System.Threading.Interlocked.Increment(ref seqCounter) & 0xFFFF);
                            var buf = BuildDeviceInfoQueryBuffer(seq); // Ĭ�� SubCmd = 0x01��Get FW Info��
                            try
                            {
                                // HidLibrary һ���ṩ Write �� WriteReport��ʹ�� Write ���Է���
                                currentHidDevice.Write(buf);
                            }
                            catch
                            {
                                // ��� Write �����û����쳣������д�����
                                try { currentHidDevice.WriteFeatureData(buf); } catch { }
                            }
                        }
                        catch
                        {
                            // ���Է��ʹ���
                        }

                        BeginReadLoop(currentHidDevice);
                    }
                }
                catch
                {
                    // ����������ȡ����
                }
            });
        }

        // ֹͣ���ͷŵ�ǰ�����豸
        private void StopListeningCurrentDevice()
        {
            lock (hidLock)
            {
                try
                {
                    if (currentHidDevice != null)
                    {
                        try
                        {
                            // ֹͣ��ȡ�� HidLibrary û����ȷ Stop API for ReadReport loop;
                            // �ر��豸�� Dispose ���ж����ڽ��еĶ�ȡ�ص�
                            currentHidDevice.CloseDevice();
                        }
                        catch { }
                        try { currentHidDevice.Dispose(); } catch { }
                        currentHidDevice = null;
                    }

                    // ���÷��ͱ�־
                    moduleQuerySent = false;
                }
                catch
                {
                    // ����
                }
            }
        }

        // ��ʼ�ݹ��ȡ���ģ�ʹ�� ReadReport �ص���
        private void BeginReadLoop(HidDevice dev)
        {
            if (dev == null) return;

            try
            {
                // ʹ���첽�ص���ȡ����
                dev.ReadReport(OnReport);
            }
            catch
            {
                // ����������ȡ����
            }
        }

        // ���Ļص����޸ģ��� MCU ������� 4 �ֽڹ̼��� 3 �ֽڳ������ڣ����ڸ�ʽ�� 15.Dec.2025��
        private void OnReport(HidReport report)
        {
            try
            {
                if (report == null) return;

                var data = report.Data ?? new byte[0];

                // HID report may include report ID at [0]; normalize
                byte[] frame;
                if (data.Length >= 21 && data[0] != 0xA5 && data[1] == 0xA5)
                    frame = data.Skip(1).ToArray(); // strip report ID
                else
                    frame = data;

                ProcessReceivedFrame(frame);

                lastReceiveTime = DateTime.Now;

                lock (hidLock)
                {
                    try { currentHidDevice?.ReadReport(OnReport); }
                    catch { }
                }
            }
            catch { }
        }

        /// <summary>
        /// Process a received 64-byte protocol frame (from HID or TCP).
        /// </summary>
        private void ProcessReceivedFrame(byte[] data)
        {
            if (data == null || data.Length < 6) return;

                // Ĭ�ϲ���ӡÿ��ԭʼ���ݣ�������־����ˢ��
                // ����ץ�����ԣ����ֶ��ָ��������У�
                // var rawText = FormatReportData(data);
                // AppendReceivedText(rawText);

                // Parse Remaining Step report (Device -> PC): [ReportId?] A5 10 01 ...
                try
                {
                    int baseIdx = -1;
                    if (data.Length >= 20 && data[0] == 0xA5 && data[1] == 0x10 && data[2] == 0x01)
                        baseIdx = 0;      // no report id included
                    else if (data.Length >= 21 && data[1] == 0xA5 && data[2] == 0x10 && data[3] == 0x01)
                        baseIdx = 1;      // report id included at [0]

                    if (baseIdx >= 0)
                    {
                        uint xRemaining = (uint)(data[baseIdx + 12]
                                               | (data[baseIdx + 13] << 8)
                                               | (data[baseIdx + 14] << 16)
                                               | (data[baseIdx + 15] << 24));

                        uint yRemaining = (uint)(data[baseIdx + 16]
                                               | (data[baseIdx + 17] << 8)
                                               | (data[baseIdx + 18] << 16)
                                               | (data[baseIdx + 19] << 24));

                        remainingStepX = xRemaining;
                        remainingStepY = yRemaining;

                        RefreshRemainLabel();

                        UpdateMotionButtonsEnabledState();
                    }
                }
                catch
                {
                    // ignore parse errors
                }

                // ���� Device Info / Module List / Module Info �ȣ��� MCU Լ��λ�ö�ȡ
                try
                {
                    if (data.Length > 5 && data[0] == 0xA5 && data[1] == 0x01)
                    {
                        byte subCmd = data.Length > 6 ? data[6] : (byte)0x00;

                        // SubCmd 0x01: Device Info (FW, Date) - ����ԭ�д������������㹻ʱ��
                        if (subCmd == 0x01 && data.Length > 13)
                        {
                            // �̼��汾��data[7..10] -> FW_VER_0..FW_VER_3
                            var fw = $"{data[7]:D3}.{data[8]:D2}.{data[9]:D2}.{data[10]:D2}";

                            // �������ڣ�data[11]=yy (���� 25 -> 2025), data[12]=month, data[13]=day
                            int year = 2000 + data[11];
                            int month = data[12];
                            int day = data[13];

                            string monthName;
                            try
                            {
                                var names = new[] { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
                                monthName = (month >= 1 && month <= 12) ? names[month - 1] : month.ToString();
                            }
                            catch
                            {
                                monthName = month.ToString();
                            }

                            var dateStr = $"{day:D2}.{monthName}.{year}";

                            // SN: bytes [14..17] as 8-char hex (same as Module SN format)
                            string deviceSn = string.Empty;
                            if (data.Length > 17)
                            {
                                deviceSn = $"{data[14]:X2}{data[15]:X2}{data[16]:X2}{data[17]:X2}";
                            }

                            // �� UI �̸߳��� Device_Info��ʹ�õ�ǰѡ���豸��Ϊ�����ģ�
                            InvokeIfRequired(() =>
                            {
                                var sel = MainDevice.SelectedItem as HidDeviceItem;
                                UpdateDeviceInfo(sel, fw, dateStr, deviceSn);
                            });

                            // Auto-query Module List (SubCmd = 0x02)
                            Task.Run(() =>
                            {
                                try
                                {
                                    if (!moduleQuerySent)
                                    {
                                        ushort seq = (ushort)(System.Threading.Interlocked.Increment(ref seqCounter) & 0xFFFF);
                                        var buf = BuildDeviceInfoQueryBuffer(seq, 0x02);
                                        SendToHID(buf.Length == 65 ? buf.Skip(1).ToArray() : buf);
                                        moduleQuerySent = true;
                                    }
                                }
                                catch { }
                            });
                        }
                        // SubCmd 0x02: Module List (MCU -> PC)
                        else if (subCmd == 0x02 && data.Length > 7)
                        {
                            // data[7] = Module_Count (number of module entries described or total instances?)
                            // ��ʽ: after [7] follows pairs of [Type][Count][Padding] for each entry.
                            try
                            {
                                int moduleCountField = data[7];
                                var parsedModules = new List<ModuleInfo>();
                                int runningIndex = 0;
                                int offset = 8;
                                // Entries are [Type][Count] pairs (2 bytes each). Parse up to moduleCountField entries.
                                for (int entry = 0; entry < moduleCountField && offset + 1 < data.Length; entry++)
                                {
                                    byte mType = data[offset];
                                    byte mCount = data[offset + 1];
                                    for (int c = 0; c < mCount; c++)
                                    {
                                        parsedModules.Add(new ModuleInfo
                                        {
                                            Index = runningIndex,
                                            Type = mType,
                                            Name = string.Empty,
                                            SN = string.Empty,
                                            FW = string.Empty,
                                            Date = string.Empty
                                        });
                                        runningIndex++;
                                    }

                                    offset += 2; // move to next entry
                                }

                                // ��������б����ȴ�ÿ��ģ�����ϸ��Ϣ���غ������������ʾ
                                InvokeIfRequired(() =>
                                {
                                    currentModules.Clear();
                                    SubDevice.Items.Clear();
                                });

                                // ��ÿ�� module ���� Get Module Info (SubCmd = 0x03) ��ѯ
                                Task.Run(() =>
                                {
                                    try
                                    {
                                        foreach (var m in parsedModules)
                                        {
                                            try
                                            {
                                                ushort seq = (ushort)(System.Threading.Interlocked.Increment(ref seqCounter) & 0xFFFF);
                                                var req = BuildDeviceInfoQueryBuffer(seq, 0x03, new byte[] { (byte)m.Index });
                                                SendToHID(req.Length == 65 ? req.Skip(1).ToArray() : req);
                                                System.Threading.Thread.Sleep(100);
                                            }
                                            catch
                                            {
                                                // ���Ե�������ʧ�ܣ�������������
                                            }
                                        }
                                    }
                                    catch
                                    {
                                        // �������巢�ʹ���
                                    }
                                });
                            }
                            catch
                            {
                                // ���Խ��� Module List ����
                            }
                        }
                        // SubCmd 0x03: Module Info �ظ� (MCU -> PC)
                        else if (subCmd == 0x03)
                        {
                            if (data.Length > 27)
                            {
                                try
                                {
                                    int moduleIndex = data[7];
                                    byte moduleType = data[8];

                                    // DeviceName bytes [9..16]
                                    int nameLen = Math.Min(8, Math.Max(0, data.Length - 9));
                                    var nameBytes = new byte[nameLen];
                                    Array.Copy(data, 9, nameBytes, 0, nameLen);
                                    string deviceName = Encoding.ASCII.GetString(nameBytes).TrimEnd('\0').Trim();

                                    // NodeID bytes [17..20] => SN display
                                    // ʾ�����ģ�... 75 68 52 16 ... -> "75685216"
                                    string snStr;
                                    uint nodeId;
                                    try
                                    {
                                        nodeId = (uint)(data[17]
                                                     | (data[18] << 8)
                                                     | (data[19] << 16)
                                                     | (data[20] << 24));

                                        // Display as hex bytes in received order: 0x75 0x68 0x2D 0x16 => "75682D16"
                                        snStr = $"{data[17]:X2}{data[18]:X2}{data[19]:X2}{data[20]:X2}";
                                    }
                                    catch
                                    {
                                        nodeId = 0;
                                        snStr = string.Empty;
                                    }

                                    // FW: data[21..24] FW3..FW_VER_0
                                    string fwStr = $"{data[21]:D3}.{data[22]:D2}.{data[23]:D2}.{data[24]:D2}";

                                    // Date: [25..27] YY MM DD
                                    int year = 2000 + data[25];
                                    int month = data[26];
                                    int day = data[27];
                                    string monthName = GetMonthNameShort(month);
                                    string dateStr = $"{day:D2}.{monthName}.{year}";

                                    // ======== Module Info V2 ��չ�� (Byte[28..43]) ========
                                    bool hasV2 = data.Length > 43;
                                    int xCurrentRaw = 0;
                                    int yCurrentRaw = 0;
                                    byte holdCurrentPct = 0;
                                    int speed = 0;
                                    int xPos = 0;
                                    int yPos = 0;

                                    if (hasV2)
                                    {
                                        try
                                        {
                                            xCurrentRaw = (ushort)(data[28] | (data[29] << 8));
                                            yCurrentRaw = (ushort)(data[30] | (data[31] << 8));
                                            holdCurrentPct = data[32];
                                            speed = (ushort)(data[34] | (data[35] << 8));
                                            xPos = (int)(data[36] | (data[37] << 8) | (data[38] << 16) | (data[39] << 24));
                                            yPos = (int)(data[40] | (data[41] << 8) | (data[42] << 16) | (data[43] << 24));
                                        }
                                        catch
                                        {
                                            hasV2 = false;
                                        }
                                    }

                                    // Occasionally device/report parsing may yield an all-zero V2 snapshot.
                                    // Treat this as invalid if we already have valid V2 data for the module.
                                    bool suspiciousZeroV2 = hasV2
                                        && xCurrentRaw == 0
                                        && yCurrentRaw == 0
                                        && holdCurrentPct == 0
                                        && speed == 0;

                                    // RX-DBG removed: async CAN reports are too frequent for the log window

                                    InvokeIfRequired(() =>
                                    {
                                        var target = currentModules.FirstOrDefault(x => x.Index == moduleIndex);
                                        if (target != null)
                                        {
                                            target.Type = moduleType;
                                            target.Name = deviceName;
                                            target.SN = snStr;
                                            target.NodeId = nodeId;
                                            target.FW = fwStr;
                                            target.Date = dateStr;
                                            target.HasV2 = hasV2;
                                            if (hasV2)
                                            {
                                                if (!(suspiciousZeroV2 && target.HasV2))
                                                {
                                                    target.XCurrentRaw = xCurrentRaw;
                                                    target.YCurrentRaw = yCurrentRaw;
                                                    target.HoldCurrentPct = holdCurrentPct;
                                                    target.Speed = speed;
                                                    target.XPos = xPos;
                                                    target.YPos = yPos;
                                                }
                                            }

                                            for (int i = 0; i < SubDevice.Items.Count; i++)
                                            {
                                                var it = SubDevice.Items[i] as ModuleInfo;
                                                if (it != null && it.Index == moduleIndex)
                                                {
                                                    SubDevice.Items[i] = target;
                                                    break;
                                                }
                                            }

                                            var sel = SubDevice.SelectedItem as ModuleInfo;
                                            if (sel != null && sel.Index == moduleIndex)
                                            {
                                                _currentNodeId = nodeId;
                                                // keep selected reference in sync in case it differs from `target`
                                                sel.Type = target.Type;
                                                sel.Name = target.Name;
                                                sel.SN = target.SN;
                                                sel.NodeId = target.NodeId;
                                                sel.FW = target.FW;
                                                sel.Date = target.Date;

                                                ShowModuleInfo(target);
                                                UpdateModuleParamDisplay(target);
                                            }

                                            // do not overwrite UI for non-selected modules
                                        }
                                        else
                                        {
                                            var newMi = new ModuleInfo
                                            {
                                                Index = moduleIndex,
                                                Type = moduleType,
                                                Name = deviceName,
                                                SN = snStr,
                                                NodeId = nodeId,
                                                FW = fwStr,
                                                Date = dateStr,
                                                HasV2 = hasV2 && !suspiciousZeroV2,
                                                XCurrentRaw = (hasV2 && !suspiciousZeroV2) ? xCurrentRaw : 0,
                                                YCurrentRaw = (hasV2 && !suspiciousZeroV2) ? yCurrentRaw : 0,
                                                HoldCurrentPct = (hasV2 && !suspiciousZeroV2) ? holdCurrentPct : (byte)0,
                                                Speed = (hasV2 && !suspiciousZeroV2) ? speed : 0,
                                                XPos = (hasV2 && !suspiciousZeroV2) ? xPos : 0,
                                                YPos = (hasV2 && !suspiciousZeroV2) ? yPos : 0
                                            };
                                            currentModules.Add(newMi);
                                            SubDevice.Items.Add(newMi);

                                            var sel = SubDevice.SelectedItem as ModuleInfo;
                                            if (sel != null && sel.Index == moduleIndex)
                                            {
                                                UpdateModuleParamDisplay(newMi);
                                            }
                                        }
                                    });
                                }
                                catch
                                {
                                    // ���Խ��� Module Info �쳣
                                }
                            }
                        }
                    }
                }
                catch
                {
                    // ���Խ����쳣������ԭ����Ϊ��
                }

        }

        // SubDevice ��ѡ��ʱ��ʾ Module_Info
        private void SubDevice_SelectedIndexChanged(object sender, EventArgs e)
        {
            try
            {
                var mi = SubDevice.SelectedItem as ModuleInfo;
                if (mi != null)
                {
                    _currentNodeId = mi.NodeId;
                    ShowModuleInfo(mi);
                    UpdateModuleParamDisplay(mi);
                    RequestModuleInfoRefresh(mi.Index);
                }
                else
                {
                    InvokeIfRequired(() => Module_Info_richTextBox.Clear());
                }
            }
            catch
            {
                // ����
            }
        }

        // �� module info ��ʾ�� Module_Info_richTextBox
        private void ShowModuleInfo(ModuleInfo mi)
        {
            InvokeIfRequired(() =>
            {
                try
                {
                    var sb = new StringBuilder();
                    sb.AppendLine($"SN: {mi.SN}");
                    sb.AppendLine($"FW: {mi.FW}");
                    sb.AppendLine($"Date: {mi.Date}");
                    var newText = sb.ToString();
                    if (string.Equals(Module_Info_richTextBox.Text.Replace("\r", ""),
                                      newText.Replace("\r", "")))
                        return;
                    Module_Info_richTextBox.Text = newText;
                }
                catch
                {
                    // ���� UI ����
                }
            });
        }

        private void RequestModuleInfoRefresh(int moduleIndex)
        {
            Task.Run(() =>
            {
                try
                {
                    var req = BuildDeviceInfoQueryBuffer(GetNextSeq(), 0x03, new byte[] { (byte)moduleIndex });
                    SendToHID(req.Length == 65 ? req.Skip(1).ToArray() : req);
                }
                catch { }
            });
        }

        private void UpdateModuleParamDisplay(ModuleInfo mi)
        {
            if (mi == null || !mi.HasV2)
                return;

            InvokeIfRequired(() =>
            {
                try
                {
                    int xCur_mA = mi.XCurrentRaw * 10;
                    int yCur_mA = mi.YCurrentRaw * 10;

                    bool hasPendingX = _pendingXCurrentUi >= 0 && _pendingXCurrentModuleIndex == mi.Index;
                    bool xConfirmed = hasPendingX && xCur_mA == _pendingXCurrentUi;
                    bool keepPendingX = hasPendingX && !xConfirmed && DateTime.Now < _pendingXCurrentUntil;
                    bool holdConfirmedX = _confirmedXCurrentUi >= 0
                        && _confirmedXCurrentModuleIndex == mi.Index
                        && DateTime.Now < _confirmedXCurrentHoldUntil;
                    bool keepConfirmedXDisplay = !keepPendingX && holdConfirmedX && xCur_mA != _confirmedXCurrentUi;

                    bool hasPendingY = _pendingYCurrentUi >= 0 && _pendingYCurrentModuleIndex == mi.Index;
                    bool yConfirmed = hasPendingY && yCur_mA == _pendingYCurrentUi;
                    bool keepPendingY = hasPendingY && !yConfirmed && DateTime.Now < _pendingYCurrentUntil;
                    bool holdConfirmedY = _confirmedYCurrentUi >= 0
                        && _confirmedYCurrentModuleIndex == mi.Index
                        && DateTime.Now < _confirmedYCurrentHoldUntil;
                    bool keepConfirmedYDisplay = !keepPendingY && holdConfirmedY && yCur_mA != _confirmedYCurrentUi;

                    bool hasPendingHold = _pendingHoldCurrentUi >= 0 && _pendingHoldCurrentModuleIndex == mi.Index;
                    bool holdConfirmed = hasPendingHold && mi.HoldCurrentPct == _pendingHoldCurrentUi;
                    bool keepPendingHold = hasPendingHold && !holdConfirmed && DateTime.Now < _pendingHoldCurrentUntil;
                    bool holdConfirmedHold = _confirmedHoldCurrentUi >= 0
                        && _confirmedHoldCurrentModuleIndex == mi.Index
                        && DateTime.Now < _confirmedHoldCurrentHoldUntil;
                    bool keepConfirmedHoldDisplay = !keepPendingHold && holdConfirmedHold && mi.HoldCurrentPct != _confirmedHoldCurrentUi;

                    if (textBox_xCurrent != null)
                    {
                        if (keepPendingX)
                            textBox_xCurrent.Text = _pendingXCurrentUi + " mA";
                        else if (keepConfirmedXDisplay)
                            textBox_xCurrent.Text = _confirmedXCurrentUi + " mA";
                        else
                            textBox_xCurrent.Text = xCur_mA.ToString();
                    }
                    if (textBox_yCurrent != null)
                    {
                        if (keepPendingY)
                            textBox_yCurrent.Text = _pendingYCurrentUi + " mA";
                        else if (keepConfirmedYDisplay)
                            textBox_yCurrent.Text = _confirmedYCurrentUi + " mA";
                        else
                            textBox_yCurrent.Text = yCur_mA.ToString();
                    }
                    if (textBox_holdCurrent != null)
                    {
                        if (keepPendingHold)
                            textBox_holdCurrent.Text = _pendingHoldCurrentUi + " %";
                        else if (keepConfirmedHoldDisplay)
                            textBox_holdCurrent.Text = _confirmedHoldCurrentUi + " %";
                        else
                            textBox_holdCurrent.Text = mi.HoldCurrentPct.ToString();
                    }
                    if (xConfirmed)
                    {
                        _confirmedXCurrentUi = _pendingXCurrentUi;
                        _confirmedXCurrentModuleIndex = mi.Index;
                        _confirmedXCurrentHoldUntil = DateTime.Now.AddMinutes(10);
                    }
                    if (yConfirmed)
                    {
                        _confirmedYCurrentUi = _pendingYCurrentUi;
                        _confirmedYCurrentModuleIndex = mi.Index;
                        _confirmedYCurrentHoldUntil = DateTime.Now.AddMinutes(10);
                    }
                    if (holdConfirmed)
                    {
                        _confirmedHoldCurrentUi = _pendingHoldCurrentUi;
                        _confirmedHoldCurrentModuleIndex = mi.Index;
                        _confirmedHoldCurrentHoldUntil = DateTime.Now.AddMinutes(10);
                    }
                    if (xConfirmed || (hasPendingX && !keepPendingX))
                    {
                        _pendingXCurrentUi = -1;
                        _pendingXCurrentModuleIndex = -1;
                    }
                    if (yConfirmed || (hasPendingY && !keepPendingY))
                    {
                        _pendingYCurrentUi = -1;
                        _pendingYCurrentModuleIndex = -1;
                    }
                    if (holdConfirmed || (hasPendingHold && !keepPendingHold))
                    {
                        _pendingHoldCurrentUi = -1;
                        _pendingHoldCurrentModuleIndex = -1;
                    }

                    bool hasPendingStep = _pendingStepSpeedUi >= 0 && _pendingStepSpeedModuleIndex == mi.Index;
                    bool stepConfirmed = hasPendingStep && mi.Speed == _pendingStepSpeedUi;
                    bool keepPending = hasPendingStep && !stepConfirmed && DateTime.Now < _pendingStepSpeedUntil;
                    bool hasConfirmedStep = _confirmedStepSpeedUi >= 0 && _confirmedStepSpeedModuleIndex == mi.Index;
                    int speedDisplayValue = mi.Speed;
                    if (mi.NodeId != 0 && _manualStepSpeedByNodeId.TryGetValue(mi.NodeId, out var manualSpeed))
                        speedDisplayValue = manualSpeed;
                    if (textBox_stepSpeed != null)
                    {
                        if (keepPending)
                            textBox_stepSpeed.Text = _pendingStepSpeedUi.ToString();
                        else if (hasConfirmedStep)
                            textBox_stepSpeed.Text = _confirmedStepSpeedUi.ToString();
                        else
                            textBox_stepSpeed.Text = speedDisplayValue.ToString();
                    }
                    if (stepConfirmed)
                    {
                        _confirmedStepSpeedUi = _pendingStepSpeedUi;
                        _confirmedStepSpeedModuleIndex = mi.Index;
                        _confirmedStepSpeedHoldUntil = DateTime.Now.AddMinutes(10);
                        try { AppendReceivedText($"[RX] StepSpeed confirmed. module={mi.Index} value={mi.Speed}\n"); } catch { }
                    }
                    else if (hasPendingStep && !keepPending)
                    {
                        try { AppendReceivedText($"[RX] StepSpeed confirm timeout. module={mi.Index} target={_pendingStepSpeedUi} device={mi.Speed}\n"); } catch { }
                    }
                    // suppress stale-step-speed log spam
                    if (stepConfirmed || (hasPendingStep && !keepPending))
                    {
                        _pendingStepSpeedUi = -1;
                        _pendingStepSpeedModuleIndex = -1;
                    }
                    UpdatePositionLabelFromRaw(mi.XPos, mi.YPos);
                }
                catch
                {
                    // ignore UI update errors
                }
            });
        }

        // �������жϻ����Ƿ�ȫ��Ϊ 0
        private static bool IsAllZeros(byte[] data)
        {
            if (data == null || data.Length == 0) return true;
            for (int i = 0; i < data.Length; i++)
            {
                if (data[i] != 0) return false;
            }
            return true;
        }

        // ��ʽ���յ�������Ϊһ���ı���ʱ�� + hex + ascii��
        private string FormatReportData(byte[] data)
        {
            var sb = new StringBuilder();
            sb.Append($"[{DateTime.Now:HH:mm:ss.fff}] ");
            if (data.Length == 0)
            {
                sb.AppendLine("<empty report>");
                return sb.ToString();
            }

            // hex
            for (int i = 0; i < data.Length; i++)
            {
                sb.Append(data[i].ToString("X2"));
                if (i < data.Length - 1) sb.Append(' ');
            }

            sb.Append("  |  ");

            // ascii
            for (int i = 0; i < data.Length; i++)
            {
                var b = data[i];
                if (b >= 32 && b <= 126)
                    sb.Append((char)b);
                else
                    sb.Append('.');
            }

            sb.AppendLine();
            return sb.ToString();
        }

        // �������ı�׷�ӵ� richTextBox1���̰߳�ȫ��
        private readonly System.Collections.Concurrent.ConcurrentQueue<string> _logQueue
            = new System.Collections.Concurrent.ConcurrentQueue<string>();
        private int _logFlushPending = 0;
        private const int LOG_MAX_LENGTH = 30000;

        private void AppendReceivedText(string text)
        {
            if (!string.IsNullOrEmpty(text))
                _logQueue.Enqueue(text);
            else if (_logQueue.IsEmpty)
                return;

            // Only schedule one UI flush at a time
            if (System.Threading.Interlocked.CompareExchange(ref _logFlushPending, 1, 0) == 0)
            {
                InvokeIfRequired(() =>
                {
                    try
                    {
                        var sb = new System.Text.StringBuilder();
                        while (_logQueue.TryDequeue(out var line))
                            sb.Append(line);

                        if (sb.Length > 0)
                        {
                            // Only auto-scroll if user isn't selecting/reading
                            bool userHasFocus = richTextBox1.Focused;

                            richTextBox1.AppendText(sb.ToString());

                            // Trim log if too long
                            if (richTextBox1.TextLength > LOG_MAX_LENGTH)
                            {
                                int removeLen = richTextBox1.TextLength - LOG_MAX_LENGTH / 2;
                                richTextBox1.Select(0, removeLen);
                                richTextBox1.SelectedText = "";
                            }

                            if (!userHasFocus)
                            {
                                richTextBox1.SelectionStart = richTextBox1.TextLength;
                                richTextBox1.ScrollToCaret();
                            }
                        }
                    }
                    catch { }
                    finally
                    {
                        System.Threading.Interlocked.Exchange(ref _logFlushPending, 0);

                        // If more items arrived during flush, schedule another
                        if (!_logQueue.IsEmpty)
                            AppendReceivedText("");
                    }
                });
            }
        }

        // ���෽�����ֲ��䣨ö��/��ȡ���к�/ע��֪ͨ/ӳ����ر���ȣ�
        // ö�ٲ��ϲ�����ͬһ�����豸���� USB �㣨�� iSerial �� USB\...\<serial>���� HID �ӿڲ㣨HID\ �� USB\...&MI_...��
        // ����ѡ��USB ������кš��� DevicePath ��ֻ��ʾһ�������б�����ֻ��ʾ���кţ�������ʾ����
        private List<HidDeviceItem> EnumerateHidViaWmiAndHidLibMergedPreferUsbSerial()
        {
            var rawList = new List<HidDeviceItem>();

            var query = new SelectQuery("Win32_PnPEntity");
            using (var searcher = new ManagementObjectSearcher(query))
            {
                foreach (ManagementObject obj in searcher.Get())
                {
                    try
                    {
                        var pnpClass = (obj["PNPClass"] ?? string.Empty).ToString();
                        if (!string.Equals(pnpClass, "HIDClass", StringComparison.OrdinalIgnoreCase))
                            continue;

                        var wmiName = (obj["Name"] ?? string.Empty).ToString();
                        var deviceId = (obj["DeviceID"] ?? string.Empty).ToString();
                        var manufacturer = (obj["Manufacturer"] ?? string.Empty).ToString();

                        int vendorId = 0, productId = 0;
                        var m = Regex.Match(deviceId ?? string.Empty, @"VID_([0-9A-Fa-f]{4}).*PID_([0-9A-Fa-f]{4})");
                        if (m.Success && m.Groups.Count >= 3)
                        {
                            int.TryParse(m.Groups[1].Value, System.Globalization.NumberStyles.HexNumber, null, out vendorId);
                            int.TryParse(m.Groups[2].Value, System.Globalization.NumberStyles.HexNumber, null, out productId);
                        }

                        // ���ˣ�ֻ����ָ���� VID/PID
                        if (vendorId != FilterVendorId || productId != FilterProductId)
                            continue;

                        string productString = null;

                        // ���ܽ����� VID/PID������ͨ�� HidLibrary ��ȡ PRODUCT_STRING
                        if (vendorId != 0 && productId != 0)
                        {
                            try
                            {
                                var hidDevices = HidDevices.Enumerate(vendorId, productId);
                                foreach (var dev in hidDevices)
                                {
                                    try
                                    {
                                        if (dev == null)
                                            continue;

                                        if (!dev.IsOpen)
                                            dev.OpenDevice();

                                        if (dev.ReadProduct(out var pData) && pData != null && pData.Length > 0)
                                        {
                                            var s = Encoding.Unicode.GetString(pData).TrimEnd('\0').Trim();
                                            if (!string.IsNullOrWhiteSpace(s))
                                            {
                                                productString = s;
                                                try { dev.CloseDevice(); } catch { }
                                                break;
                                            }
                                        }
                                    }
                                    catch
                                    {
                                        // ���Ե����豸��ȡʧ�ܣ��������������ӿ�
                                    }
                                    finally
                                    {
                                        try { dev.CloseDevice(); } catch { }
                                        try { dev.Dispose(); } catch { }
                                    }
                                }
                            }
                            catch
                            {
                                // HidLibrary ö�ٻ��ȡʧ�ܣ����˵� WMI name
                                productString = null;
                            }
                        }

                        var displayNameSource = string.IsNullOrWhiteSpace(productString) ? wmiName : productString;

                        var item = new HidDeviceItem
                        {
                            DevicePath = deviceId,
                            VendorId = vendorId,
                            ProductId = productId,
                            Product = displayNameSource,
                            Manufacturer = manufacturer,
                            DisplayName = string.IsNullOrWhiteSpace(displayNameSource) ? $"HID {vendorId:X4}:{productId:X4}" : $"{displayNameSource} ({vendorId:X4}:{productId:X4})"
                        };

                        rawList.Add(item);
                    }
                    catch
                    {
                        // ���Ե�����¼���󣬼���ö�������豸
                    }
                }
            }

            // �� Vendor:Product:ProductString ���飬Ȼ����ÿ��������ѡ�� USB ���Ҵ����кŵ���Ŀ����ʽΪ USB\VID_xxxx&PID_xxxx\<serial>��serial ���ֲ��� '&'��
            var mergedList = new List<HidDeviceItem>();

            // Group by vendor/product/productstring to find identical product groups
            var groups = rawList.GroupBy(i => $"{i.VendorId:X4}:{i.ProductId:X4}:{(i.Product ?? string.Empty).Trim()}").ToList();
            foreach (var g in groups)
            {
                var items = g.ToList();

                // If any item has an extracted serial, keep the existing collapse behavior and prefer USB-with-serial
                if (items.Any(x => !string.IsNullOrWhiteSpace(ExtractSerialFromDevicePath(x.DevicePath))))
                {
                    Func<HidDeviceItem, bool> isUsbWithSerial = it =>
                    {
                        if (string.IsNullOrWhiteSpace(it.DevicePath))
                            return false;
                        if (!it.DevicePath.StartsWith("USB\\", StringComparison.OrdinalIgnoreCase))
                            return false;
                        var lastSegment = it.DevicePath.Split('\\').LastOrDefault();
                        if (string.IsNullOrWhiteSpace(lastSegment))
                            return false;
                        return !lastSegment.Contains("&");
                    };

                    var preferred = items.FirstOrDefault(isUsbWithSerial) ?? items.FirstOrDefault(x => x.DevicePath != null && x.DevicePath.StartsWith("USB\\", StringComparison.OrdinalIgnoreCase)) ?? items[0];

                    var manufacturer = items.FirstOrDefault(x => !string.IsNullOrWhiteSpace(x.Manufacturer) && !x.Manufacturer.StartsWith("("))?.Manufacturer
                                       ?? preferred.Manufacturer;

                    string serial = ExtractSerialFromDevicePath(preferred.DevicePath);

                    string displayName = !string.IsNullOrWhiteSpace(serial)
                        ? serial
                        : (string.IsNullOrWhiteSpace(preferred.Product) ? $"HID {preferred.VendorId:X4}:{preferred.ProductId:X4}" : $"{preferred.Product} ({preferred.VendorId:X4}:{preferred.ProductId:X4})");

                    mergedList.Add(new HidDeviceItem
                    {
                        DevicePath = preferred.DevicePath,
                        VendorId = preferred.VendorId,
                        ProductId = preferred.ProductId,
                        Product = preferred.Product,
                        Manufacturer = manufacturer,
                        DisplayName = displayName,
                        Serial = serial
                    });
                }
                else
                {
                    // No serials in this group: keep each interface separately (do not merge)
                    foreach (var it in items)
                    {
                        string displayName = string.IsNullOrWhiteSpace(it.Serial)
                            ? (string.IsNullOrWhiteSpace(it.Product) ? $"HID {it.VendorId:X4}:{it.ProductId:X4}" : $"{it.Product} ({it.VendorId:X4}:{it.ProductId:X4})")
                            : it.Serial;

                        // To ensure uniqueness when product string is identical, append a short suffix from the DevicePath
                        string suffix = string.Empty;
                        try
                        {
                            var last = (it.DevicePath ?? string.Empty).Split('\\').LastOrDefault() ?? string.Empty;
                            if (!string.IsNullOrWhiteSpace(last))
                            {
                                // shorten if it's long
                                suffix = last.Length > 8 ? last.Substring(last.Length - 8) : last;
                                displayName = displayName + " - " + suffix;
                            }
                        }
                        catch { }

                        mergedList.Add(new HidDeviceItem
                        {
                            DevicePath = it.DevicePath,
                            VendorId = it.VendorId,
                            ProductId = it.ProductId,
                            Product = it.Product,
                            Manufacturer = it.Manufacturer,
                            DisplayName = displayName,
                            Serial = it.Serial
                        });
                    }
                }
            }

            return mergedList.OrderBy(i => i.DisplayName).ToList();
        }

        // �� DevicePath ��ȡ���кţ��� USB\VID_xxxx&PID_xxxx\<serial> ���ָ�ʽ���������һ�Σ������� '&' �������кţ�
        private static string ExtractSerialFromDevicePath(String devicePath)
        {
            if (string.IsNullOrWhiteSpace(devicePath))
                return null;

            try
            {
                var parts = devicePath.Split('\\');
                var last = parts.LastOrDefault();
                if (string.IsNullOrWhiteSpace(last))
                    return null;
                // �ų����� '&' �ĶΣ�ͨ��Ϊ������Ϣ���� MI_ �� VID_...&PID_...��
                if (last.Contains("&"))
                    return null;
                return last;
            }
            catch
            {
                return null;
            }
        }

        // ע���豸֪ͨ��USB device interface��
        private void RegisterForDeviceNotifications()
        {
            try
            {
                // ���� DEV_BROADCAST_DEVICEINTERFACE �ṹ
                var filter = new DEV_BROADCAST_DEVICEINTERFACE
                {
                    dbcc_size = Marshal.SizeOf(typeof(DEV_BROADCAST_DEVICEINTERFACE)),
                    dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE,
                    dbcc_reserved = 0,
                    dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE
                };

                IntPtr buffer = Marshal.AllocHGlobal(filter.dbcc_size);
                try
                {
                    Marshal.StructureToPtr(filter, buffer, false);
                    deviceNotificationHandle = RegisterDeviceNotification(this.Handle, buffer, DEVICE_NOTIFY_WINDOW_HANDLE);
                    // RegisterDeviceNotification �Ḵ�ƽṹ�壬�����ͷź�����ע���õ� pointer���Կ��ͷű��� buffer��
                }
                finally
                {
                    Marshal.FreeHGlobal(buffer);
                }
            }
            catch
            {
                // ����ע��ʧ�ܣ����˵� WM_DEVICECHANGE ȫ����Ϣ������
            }
        }

        private void UnregisterDeviceNotificationSafe()
        {
            try
            {
                if (deviceNotificationHandle != IntPtr.Zero)
                {
                    UnregisterDeviceNotification(deviceNotificationHandle);
                    deviceNotificationHandle = IntPtr.Zero;
                }
            }
            catch
            {
                // ����ע��ʧ��
            }
        }

        // ����ӳ���ļ����򵥵� key<TAB>value ÿ�и�ʽ��
        private void LoadNameMappingsSafe()
        {
            try
            {
                if (!File.Exists(mappingsFilePath))
                    return;

                var lines = File.ReadAllLines(mappingsFilePath, Encoding.UTF8);
                nameMappings.Clear();
                foreach (var line in lines)
                {
                    if (string.IsNullOrWhiteSpace(line))
                        continue;
                    var idx = line.IndexOf('\t');
                    if (idx <= 0) continue;
                    var key = line.Substring(0, idx).Trim();
                    var val = line.Substring(idx + 1);
                    if (!string.IsNullOrWhiteSpace(key))
                        nameMappings[key] = val;
                }
            }
            catch
            {
                // ���Լ��ش���
            }
        }

        // ����ӳ���ļ������ǣ�
        private void SaveNameMappingsSafe()
        {
            try
            {
                var dir = Path.GetDirectoryName(mappingsFilePath);
                if (!Directory.Exists(dir))
                    Directory.CreateDirectory(dir);

                var sb = new StringBuilder();
                foreach (var kv in nameMappings)
                {
                    var key = kv.Key.Replace("\t", " ").Replace("\r", "").Replace("\n", " ");
                    var val = kv.Value.Replace("\t", " ").Replace("\r", "").Replace("\n", " ");
                    sb.AppendLine($"{key}\t{val}");
                }

                File.WriteAllText(mappingsFilePath, sb.ToString(), Encoding.UTF8);
            }
            catch
            {
                // ���Ա������
            }
        }

        // �򵥵� UI �̵߳��ȸ���
        private void InvokeIfRequired(Action action)
        {
            if (this.IsHandleCreated && this.InvokeRequired)
                this.BeginInvoke(action);
            else
                action();
        }

        #region PInvoke

        [StructLayout(LayoutKind.Sequential)]
        private struct DEV_BROADCAST_DEVICEINTERFACE
        {
            public int dbcc_size;
            public int dbcc_devicetype;
            public int dbcc_reserved;
            public Guid dbcc_classguid;
            // dbcc_name ��ṹ������ɱ䳤������Ҫ��������������ע��
        }

        [DllImport("user32.dll", SetLastError = true)]
        private static extern IntPtr RegisterDeviceNotification(IntPtr hRecipient, IntPtr NotificationFilter, uint Flags);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool UnregisterDeviceNotification(IntPtr Handle);

        #endregion

        // �򵥵ĵ�������Ի������� Change Name��
        private sealed class SingleLinePrompt : Form
        {
            private readonly TextBox tb;
            public string Value => tb.Text;

            public SingleLinePrompt(string title, string label, string initial = "")
                : this(title, label, initial, null)
            {
            }

            public SingleLinePrompt(string title, string label, string initial, int? maxLength)
            {
                Text = title;
                StartPosition = FormStartPosition.CenterParent;
                FormBorderStyle = FormBorderStyle.FixedDialog;
                MaximizeBox = false;
                MinimizeBox = false;
                ShowInTaskbar = false;
                Width = 400;
                Height = 140;

                var lbl = new Label() { Left = 12, Top = 12, Text = label, AutoSize = true };
                tb = new TextBox() { Left = 12, Top = 36, Width = 360, Text = initial ?? "" };
                if (maxLength.HasValue && maxLength.Value > 0)
                {
                    tb.MaxLength = maxLength.Value;
                }

                var btnOk = new Button() { Text = "OK", DialogResult = DialogResult.OK, Left = 220, Width = 75, Top = 70 };
                var btnCancel = new Button() { Text = "Cancel", DialogResult = DialogResult.Cancel, Left = 300, Width = 75, Top = 70 };

                btnOk.Anchor = AnchorStyles.Bottom | AnchorStyles.Right;
                btnCancel.Anchor = AnchorStyles.Bottom | AnchorStyles.Right;

                Controls.Add(lbl);
                Controls.Add(tb);
                Controls.Add(btnOk);
                Controls.Add(btnCancel);

                AcceptButton = btnOk;
                CancelButton = btnCancel;
            }
        }

        private sealed class HidDeviceItem
        {
            public string DisplayName { get; set; }
            public string DevicePath { get; set; }
            public int VendorId { get; set; }
            public int ProductId { get; set; }
            public string Manufacturer { get; set; }
            public string Product { get; set; }
            public string Serial { get; set; }
            public override string ToString() => DisplayName;
        }

        private sealed class EthDeviceItem
        {
            public string Hostname { get; set; }
            public string IP { get; set; }
            public int TcpPort { get; set; }
            public byte[] MAC { get; set; }
            public string FW { get; set; }
            public DateTime LastSeen { get; set; }
            public override string ToString() => Hostname;
        }

        // ModuleInfo ������ SubDevice �б�����ʾ Module_Info
        private sealed class ModuleInfo
        {
            public int Index { get; set; }
            public byte Type { get; set; }
            public string Name { get; set; }
            public string SN { get; set; }
            public uint NodeId { get; set; }
            public string FW { get; set; }
            public string Date { get; set; }

            public bool HasV2 { get; set; }
            public int XCurrentRaw { get; set; }
            public int YCurrentRaw { get; set; }
            public byte HoldCurrentPct { get; set; }
            public int Speed { get; set; }
            public int XPos { get; set; }
            public int YPos { get; set; }

            public string CustomName { get; set; }

            public override string ToString()
            {
                var category = ModuleTypeToCategoryName(Type);

                var displayName = !string.IsNullOrWhiteSpace(CustomName) ? CustomName : Name;
                if (!string.IsNullOrWhiteSpace(displayName))
                    return $"[{category}] {displayName}";

                return $"[{category}] Module #{Index}";
            }
        }

        // �� Form1 �������Ӵ˷��������������������
        private ushort GetNextSeq()
        {
            return (ushort)(System.Threading.Interlocked.Increment(ref seqCounter) & 0xFFFF);
        }

        // �� Form1 �������ӣ����� Device_Info richtextbox ���ݣ���ʾ SN / FW / Date��
        private void UpdateDeviceInfo(HidDeviceItem item, string fw = null, string date = null, string sn = null)
        {
            InvokeIfRequired(() =>
            {
                try
                {
                    var snVal = sn ?? item?.Serial ?? string.Empty;

                    var fwVal = fw ?? string.Empty;
                    var dateVal = date ?? string.Empty;

                    var sb = new StringBuilder();
                    sb.AppendLine($"SN: {snVal}");
                    if (!string.IsNullOrEmpty(_pendingEthIp))
                        sb.AppendLine($"IP: {_pendingEthIp}");
                    sb.AppendLine($"FW: {fwVal}");
                    sb.AppendLine($"Date: {dateVal}");

                    try
                    {
                        var rtb = this.Controls.Find("Device_Info", true).FirstOrDefault() as RichTextBox;
                        if (rtb != null)
                        {
                            rtb.Text = sb.ToString();
                        }
                    }
                    catch { }
                }
                catch { }
            });
        }

        // �°棺Ϊ HID д����뱨�� ID ǰ׺��report ID = 0��
        private byte[] BuildDeviceInfoQueryBuffer(ushort seq)
        {
            return BuildDeviceInfoQueryBuffer(seq, 0x01);
        }

        // �������أ�����ָ�� SubCmd������ 0x01=Get FW, 0x02=Query Module, 0x03=Get Module Info��
        private byte[] BuildDeviceInfoQueryBuffer(ushort seq, byte subCmd)
        {
            return BuildDeviceInfoQueryBuffer(seq, subCmd, null);
        }

        // �������أ�ָ�� SubCmd �ҿɴ� payload��payload ��ÿ���ֽڽ����� buf[8..]��
        private byte[] BuildDeviceInfoQueryBuffer(ushort seq, byte subCmd, byte[] payload)
        {
            // HidLibrary �� Write ͨ��������һ���ֽ�Ϊ report ID��û�� report ʱΪ 0x00��
            var buf = new byte[65]; // +1 ���� report ID
            buf[0] = 0x00;          // Report ID = 0
            buf[1] = 0xA5;
            buf[2] = 0x01;     // CMD_DEVICE_INFO
            buf[3] = 0x02;     // PC �� MCU
            buf[4] = (byte)(seq & 0xFF);
            buf[5] = (byte)(seq >> 8);
            if (payload != null && payload.Length > 0)
            {
                buf[6] = (byte)payload.Length; // payload length
            }
            else
            {
                buf[6] = 0x01; // default payload length (SubCmd only)
            }
            buf[7] = subCmd;  // SubCmd
            if (payload != null && payload.Length > 0)
            {
                // put payload starting at index 8
                Array.Copy(payload, 0, buf, 8, Math.Min(payload.Length, buf.Length - 8));
            }
            // �����ֽڱ���Ϊ 0
            return buf;
        }

        // �°棺�� Module_Type �Rӳ��Ϊ������
        private static string ModuleTypeToCategoryName(byte t)
        {
            switch (t)
            {
                case 0x10: return "Laser Driver";
                case 0x11: return "Galvo";
                case 0x12: return "Adj";
                case 0x20: return "Fan";
                case 0x21: return "Defogger";
                case 0x22: return "Dehumidifier";
                case 0x23: return "Heater";
                default: return $"Unknown(0x{t:X2})";
            }
        }

        // ��ȡ����������ǰ�� Device Info ����һ�£�
        private static string GetMonthNameShort(int month)
        {
            try
            {
                var names = new[] { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
                return (month >= 1 && month <= 12) ? names[month - 1] : month.ToString();
            }
            catch
            {
                return month.ToString();
            }
        }

    // ���������豸����ʱ�������ö�٣����� WMI/ϵͳע���ͺ󣩣�ʹ��ͬ�� Invoke ����ö�ٲ���ȡ�б�����
    private void StartArrivalRetry()
    {
        Task.Run(async () =>
        {
            const int attempts = 6;
            const int delayMs = 300;

            for (int i = 0; i < attempts; i++)
            {
                try
                {
                    // �� UI �߳�ͬ������һ��ö�٣����� BeginInvoke ���첽�ӳ٣�
                    if (this.IsHandleCreated && this.InvokeRequired)
                    {
                        this.Invoke((Action)(() => PopulateHidDevicesAsync()));
                    }
                    else
                    {
                        PopulateHidDevicesAsync();
                    }
                }
                catch
                {
                    // ��������ö����쳣
                }

                await Task.Delay(delayMs).ConfigureAwait(false);

                try
                {
                    int count = 0;
                    if (this.IsHandleCreated && this.InvokeRequired)
                    {
                        count = (int)this.Invoke(new Func<int>(() => MainDevice.Items.Count));
                    }
                    else
                    {
                        count = MainDevice.Items.Count;
                    }

                    if (count > 0)
                        break; // �ҵ��豸��ֹͣ����
                }
                catch
                {
                    // ���Զ�ȡ UI ʱ���쳣����������
                }
            }
        });
    }

        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {

        }

        private void radioButton4_CheckedChanged(object sender, EventArgs e)
        {

        }

        private void button_left_Click(object sender, EventArgs e)
        {
            SendMotionCommandFromButton(MotionButton.XPlus);
        }

        private void button_right_Click(object sender, EventArgs e)
        {
            SendMotionCommandFromButton(MotionButton.XMinus);
        }

        private void button_up_Click(object sender, EventArgs e)
        {
            SendMotionCommandFromButton(MotionButton.YPlus);
        }

        private void button_down_Click(object sender, EventArgs e)
        {
            SendMotionCommandFromButton(MotionButton.YMinus);
        }

        private enum MotionButton
        {
            XPlus,
            XMinus,
            YPlus,
            YMinus
        }

        private static void MapMotion(MotionButton button, out byte axis, out byte dirBit0)
        {
            // Keep the mapping exactly as required:
            // X+ => axis 0, dir 0
            // X- => axis 0, dir 1
            // Y+ => axis 1, dir 0
            // Y- => axis 1, dir 1
            switch (button)
            {
                case MotionButton.XPlus:
                    axis = 0;
                    dirBit0 = 0;
                    break;
                case MotionButton.XMinus:
                    axis = 0;
                    dirBit0 = 1;
                    break;
                case MotionButton.YPlus:
                    axis = 1;
                    dirBit0 = 0;
                    break;
                case MotionButton.YMinus:
                    axis = 1;
                    dirBit0 = 1;
                    break;
                default:
                    axis = 0;
                    dirBit0 = 0;
                    break;
            }
        }

        private void SendMotionCommandFromButton(MotionButton button)
        {
            byte axis;
            byte dirBit0;
            MapMotion(button, out axis, out dirBit0);
            ApplyMotionMapping(ref axis, ref dirBit0);
            SendMotionCommand(axis, dirBit0);
        }

        private void ApplyMotionMapping(ref byte axis, ref byte dirBit0)
        {
            bool swapXY = checkBoxXY != null && checkBoxXY.Checked;
            bool invertX = checkBoxX != null && checkBoxX.Checked;
            bool invertY = checkBoxY != null && checkBoxY.Checked;

            // 1) optional axis swap
            if (swapXY)
            {
                if (axis == 0) axis = 1;
                else if (axis == 1) axis = 0;
            }

            // 2) optional direction invert on final target axis
            if (axis == 0 && invertX)
                dirBit0 ^= 0x01;
            else if (axis == 1 && invertY)
                dirBit0 ^= 0x01;
        }

        private void SendMotionCommand(int axis, int direction)
        {
            // Ensure we only ever use the mapped values for BOTH log and frame bytes.
            byte axisByte = (byte)axis;
            byte dirBit0 = (byte)(direction & 0x01);

            uint nodeId = GetCurrentNodeID(); // ��ȡNodeID
            if (nodeId == 0)
            {
                try
                {
                    AppendReceivedText("[TX] MOTION skipped: nodeId is 0 (no valid module selected).\n");
                }
                catch { }
                return;
            }

            byte[] cmd = new byte[64];
            cmd[0] = 0xA5; // Frame head
            cmd[1] = 0x10; // CMD_MOTION (NEW)
            cmd[2] = 0x02; // DIR_PC_TO_MCU

            ushort seq = GetNextSeq(); // ʵ���������
            cmd[3] = (byte)(seq & 0xFF);
            cmd[4] = (byte)((seq >> 8) & 0xFF);

            // NodeID (MCU expectation): big-endian (MSB first)
            cmd[5] = (byte)((nodeId >> 24) & 0xFF);
            cmd[6] = (byte)((nodeId >> 16) & 0xFF);
            cmd[7] = (byte)((nodeId >> 8) & 0xFF);
            cmd[8] = (byte)(nodeId & 0xFF);

            cmd[9] = axisByte;        // Axis MUST be written ONLY here
            cmd[10] = dirBit0;        // Direction MUST be written ONLY to bit0 (other bits remain 0)
            cmd[11] = 0;              // Reserved

            int step = GetStepValue();
            cmd[12] = (byte)(step & 0xFF);
            cmd[13] = (byte)((step >> 8) & 0xFF);
            cmd[14] = (byte)((step >> 16) & 0xFF);
            cmd[15] = (byte)((step >> 24) & 0xFF);

            // [16..63] Reserved = 0 (�ѳ�ʼ��)

            try
            {
                AppendReceivedText($"[TX] MOTION axis={axisByte} dir={dirBit0} step={step} nodeId={nodeId} seq={seq}\n");
            }
            catch { }

            SendToHID(cmd); // ֱ�ӷ��͵�HID
}

        // �� Form1 �������Ӵ˷��������ڻ�ȡ��ǰ NodeID���ɸ���ʵ�������޸�ʵ�ַ�ʽ��
        private uint GetCurrentNodeID()
        {
            // NodeID MUST come from the currently selected module (parsed from ModuleInfo bytes).
            var mi = SubDevice.SelectedItem as ModuleInfo;
            if (mi != null && mi.NodeId != 0)
                return mi.NodeId;

            // fallback to last known id
            if (_currentNodeId != 0)
                return _currentNodeId;

            return 0;
        }

        // �� Form1 �������� SendToHID ����
        private void SendToHID(byte[] data)
        {
            // TCP mode: send via TcpTransport
            if (_usingTcp && _tcpTransport != null && _tcpTransport.IsConnected)
            {
                try
                {
                    AppendReceivedText("[TX-TCP] " + FormatReportData(data));
                }
                catch { }

                _tcpTransport.Write(data);
                return;
            }

            // HID mode
            lock (hidLock)
            {
                try
                {
                    if (currentHidDevice != null && currentHidDevice.IsOpen && currentHidDevice.IsConnected)
                    {
                        byte[] buf;
                        if (data.Length == 64)
                        {
                            buf = new byte[65];
                            buf[0] = 0x00;
                            Array.Copy(data, 0, buf, 1, 64);
                        }
                        else if (data.Length == 65)
                        {
                            buf = data;
                        }
                        else
                        {
                            return;
                        }

                        try
                        {
                            AppendReceivedText("[TX] " + FormatReportData(buf.Skip(1).ToArray()));
                        }
                        catch { }

                        currentHidDevice.Write(buf);
                    }
                    else
                    {
                        try
                        {
                            AppendReceivedText("[TX] Skip: device not connected.\n");
                        }
                        catch { }
                    }
                }
                catch { }
            }
        }

        // �� Form1 �������� RefreshDisplayNames ����
        // =========================================================
        // TCP Connection
        // =========================================================
        private async void ConnectTcp(string host, int port)
        {
            try
            {
                if (_tcpTransport != null)
                {
                    _tcpTransport.FrameReceived -= OnTcpFrameReceived;
                    _tcpTransport.Disconnected -= OnTcpDisconnected;
                    _tcpTransport.Dispose();
                }

                _tcpTransport = new TcpTransport();
                _tcpTransport.FrameReceived += OnTcpFrameReceived;
                _tcpTransport.Disconnected += OnTcpDisconnected;

                AppendReceivedText($"[TCP] Connecting to {host}:{port} ...\n");

                await _tcpTransport.ConnectAsync(host, port);

                _usingTcp = true;
                _transport = _tcpTransport;
                moduleQuerySent = false;

                AppendReceivedText("[TCP] Connected!\n");

                // Auto-query device info
                Task.Run(() =>
                {
                    try
                    {
                        var buf = BuildDeviceInfoQueryBuffer(GetNextSeq(), 0x01);
                        SendToHID(buf);
                    }
                    catch { }
                });
            }
            catch (Exception ex)
            {
                AppendReceivedText($"[TCP] Connect failed: {ex.Message}\n");
                _usingTcp = false;
            }
        }

        private void DisconnectTcp()
        {
            _usingTcp = false;
            if (_tcpTransport != null)
            {
                _tcpTransport.FrameReceived -= OnTcpFrameReceived;
                _tcpTransport.Disconnected -= OnTcpDisconnected;
                _tcpTransport.Dispose();
                _tcpTransport = null;
            }

            InvokeIfRequired(() =>
            {
                currentModules.Clear();
                SubDevice.Items.Clear();
            });

            AppendReceivedText("[TCP] Disconnected.\n");
        }

        private void OnTcpFrameReceived(byte[] frame)
        {
            try
            {
                ProcessReceivedFrame(frame);
            }
            catch { }
        }

        private void OnTcpDisconnected()
        {
            _usingTcp = false;
            AppendReceivedText("[TCP] Connection lost.\n");
        }

        // =========================================================
        // UDP Device Discovery (passive: listen for MCU broadcasts)
        // =========================================================
        private void StartEthDiscovery()
        {
            try
            {
                _discoveryClient = new UdpClient();
                _discoveryClient.Client.SetSocketOption(
                    System.Net.Sockets.SocketOptionLevel.Socket,
                    System.Net.Sockets.SocketOptionName.ReuseAddress, true);
                _discoveryClient.Client.Bind(new System.Net.IPEndPoint(System.Net.IPAddress.Any, UDP_DISCOVERY_PORT));
                BeginDiscoveryReceive();
            }
            catch (Exception ex)
            {
                AppendReceivedText($"[Discovery] Failed to bind UDP port {UDP_DISCOVERY_PORT}: {ex.Message}\n");
            }

            // Cleanup timer: remove stale devices
            _discoveryCleanupTimer = new System.Windows.Forms.Timer();
            _discoveryCleanupTimer.Interval = 5000;
            _discoveryCleanupTimer.Tick += DiscoveryCleanup_Tick;
            _discoveryCleanupTimer.Start();
        }

        private void BeginDiscoveryReceive()
        {
            try
            {
                _discoveryClient?.BeginReceive(OnDiscoveryReceive, null);
            }
            catch { }
        }

        private void OnDiscoveryReceive(IAsyncResult ar)
        {
            try
            {
                var ep = new System.Net.IPEndPoint(System.Net.IPAddress.Any, 0);
                byte[] data = _discoveryClient?.EndReceive(ar, ref ep);

                if (data != null && data.Length >= 32
                    && data[0] == (byte)'D' && data[1] == (byte)'T'
                    && data[2] == (byte)'D' && data[3] == (byte)'R')
                {
                    var mac = new byte[6];
                    Array.Copy(data, 4, mac, 0, 6);

                    int hostLen = 0;
                    for (int i = 10; i < 26 && data[i] != 0; i++) hostLen++;
                    string hostname = System.Text.Encoding.ASCII.GetString(data, 10, hostLen);

                    string fw = $"{data[26]:D3}.{data[27]:D2}.{data[28]:D2}.{data[29]:D2}";
                    int tcpPort = data[30] | (data[31] << 8);
                    string ip = ep.Address.ToString();

                    InvokeIfRequired(() => UpdateDiscoveredDevice(hostname, ip, tcpPort, mac, fw));
                }
            }
            catch { }

            BeginDiscoveryReceive();
        }

        private void UpdateDiscoveredDevice(string hostname, string ip, int tcpPort, byte[] mac, string fw)
        {
            string macStr = BitConverter.ToString(mac);

            // Find existing by MAC
            EthDeviceItem existing = null;
            int existingIdx = -1;
            for (int i = 0; i < MainDevice.Items.Count; i++)
            {
                if (MainDevice.Items[i] is EthDeviceItem ei && BitConverter.ToString(ei.MAC) == macStr)
                {
                    existing = ei;
                    existingIdx = i;
                    break;
                }
            }

            if (existing != null)
            {
                bool nameChanged = existing.Hostname != hostname;
                existing.IP = ip;
                existing.Hostname = hostname;
                existing.TcpPort = tcpPort;
                existing.FW = fw;
                existing.LastSeen = DateTime.UtcNow;

                // Refresh ListBox text when hostname changed (e.g. after device rename)
                if (nameChanged && existingIdx >= 0 && existingIdx != MainDevice.SelectedIndex)
                    MainDevice.Items[existingIdx] = existing;
            }
            else
            {
                var item = new EthDeviceItem
                {
                    Hostname = hostname,
                    IP = ip,
                    TcpPort = tcpPort,
                    MAC = mac,
                    FW = fw,
                    LastSeen = DateTime.UtcNow
                };
                MainDevice.Items.Add(item);
            }
        }

        private void DiscoveryCleanup_Tick(object sender, EventArgs e)
        {
            // Remove stale ETH devices (not seen for 15s)
            var now = DateTime.UtcNow;
            for (int i = MainDevice.Items.Count - 1; i >= 0; i--)
            {
                if (MainDevice.Items[i] is EthDeviceItem ei
                    && (now - ei.LastSeen).TotalSeconds > 15)
                {
                    bool wasSelected = (MainDevice.SelectedIndex == i);
                    MainDevice.Items.RemoveAt(i);
                    if (wasSelected)
                        DisconnectTcp();
                }
            }
        }

        private void RefreshDisplayNames()
        {
            InvokeIfRequired(() =>
            {
                for (int i = 0; i < MainDevice.Items.Count; i++)
                {
                    if (MainDevice.Items[i] is HidDeviceItem item)
                    {
                        if (!string.IsNullOrWhiteSpace(item.Serial) && nameMappings.TryGetValue(item.Serial, out var mapped))
                        {
                            item.DisplayName = mapped;
                        }
                        else
                        {
                            item.DisplayName = !string.IsNullOrWhiteSpace(item.Serial)
                                ? item.Serial
                                : (string.IsNullOrWhiteSpace(item.Product)
                                    ? $"HID {item.VendorId:X4}:{item.ProductId:X4}"
                                    : $"{item.Product} ({item.VendorId:X4}:{item.ProductId:X4})");
                        }
                        MainDevice.Items[i] = item; // ���� ListBox ����
                    }
                }
            });
        }

        // ���Ӵ˷����� Form1 ����
        private void Device_Info_TextChanged(object sender, EventArgs e)
        {
            // ���Ը�����Ҫ���Ӵ����߼�����ʱ����
}

        private void button_clearLog_Click(object sender, EventArgs e)
        {
            try
            {
                richTextBox1.Clear();
            }
            catch
            {
                // ignore
            }
        }

        private int GetStepValue()
        {
            // Prefer explicit step radios first (existing behavior)
            for (int i = 1; i <= 10000; i++)
            {
                var radio = this.Controls.Find($"radio_x{i}", true).FirstOrDefault() as RadioButton;
                if (radio != null && radio.Checked)
                    return i;
            }

            // New: support multiplier radios like radio_x3, radio_x5
            // This allows convenient step sizes beyond powers of 10 when added in the designer.
            try
            {
                var candidates = new[] { 3, 5 };
                foreach (var m in candidates)
                {
                    var radio = this.Controls.Find($"radio_x{m}", true).FirstOrDefault() as RadioButton;
                    if (radio != null && radio.Checked)
                        return m;
                }
            }
            catch
            {
                // ignore
            }

            return 1;
        }

        private void SendParamSet(ParamId id, int rawValue)
        {
            try
            {
                ushort seq = GetNextSeq();

                uint nodeId = GetCurrentNodeID();
                if (nodeId == 0)
                {
                    try { AppendReceivedText("[TX] PARAM_SET skipped: nodeId is 0 (no valid module selected).\n"); } catch { }
                    return;
                }

                var frame = new byte[64];
                frame[0] = 0xA5;
                frame[1] = CMD_PARAM_SET;
                frame[2] = 0x02; // PC -> Device
                frame[3] = (byte)(seq & 0xFF);
                frame[4] = (byte)((seq >> 8) & 0xFF);

                // payload is fixed: 4 (NodeID) + 1 (param_id) + 2 (value)
                frame[5] = 0x07;

                frame[6] = (byte)(nodeId & 0xFF);
                frame[7] = (byte)((nodeId >> 8) & 0xFF);
                frame[8] = (byte)((nodeId >> 16) & 0xFF);
                frame[9] = (byte)((nodeId >> 24) & 0xFF);

                frame[10] = (byte)id;

                if (id == ParamId.Speed)
                {
                    _manualStepSpeedByNodeId[nodeId] = rawValue;
                    InvokeIfRequired(() =>
                    {
                        var cm = currentModules.FirstOrDefault(m => m.NodeId == nodeId);
                        if (cm != null)
                            cm.Speed = rawValue;

                        var selMi = SubDevice.SelectedItem as ModuleInfo;
                        if (selMi != null && selMi.NodeId == nodeId)
                            UpdateModuleParamDisplay(selMi);
                    });
                }

                ushort v;
                switch (id)
                {
                    case ParamId.Hold:
                        v = (ushort)Clamp(rawValue, 0, 100);
                        break;
                    default:
                        v = (ushort)Math.Max(0, Math.Min(0xFFFF, rawValue));
                        break;
                }

                frame[11] = (byte)(v & 0xFF);
                frame[12] = (byte)((v >> 8) & 0xFF);

                // HidLibrary requires report id prefix when sending
                var buf = new byte[65];
                buf[0] = 0x00;
                Array.Copy(frame, 0, buf, 1, 64);

                try
                {
                    // show full 64-byte payload in log (skip report id)
                    var log = FormatReportData(buf.Skip(1).ToArray());
                    AppendReceivedText($"[TX] PARAM_SET id=0x{((byte)id):X2} raw={rawValue} nodeId={nodeId} seq={seq} -> {log}");
                }
                catch { }

                _transport.Write(buf);
            }
            catch
            {
                // ignore
            }
        }

        private static int ParseLeadingInt(string text)
        {
            if (string.IsNullOrWhiteSpace(text))
                return 0;

            var m = Regex.Match(text, @"-?\d+");
            if (!m.Success)
                return 0;

            int v;
            if (int.TryParse(m.Value, out v))
                return v;
            return 0;
        }

        private static int Clamp(int v, int min, int max)
        {
            if (v < min) return min;
            if (v > max) return max;
            return v;
        }

        // X Cur / Y Cur: UI shows (raw * 10). Sending MUST use raw (UI / 10).
        private void button_xCurrentPlus_Click(object sender, EventArgs e)
        {
            int ui = ParseLeadingInt(textBox_xCurrent.Text);
            ui = Clamp(ui + 10, 0, 655350);
            textBox_xCurrent.Text = ui + " mA";
            _pendingXCurrentUi = ui;
            _pendingXCurrentUntil = DateTime.Now.AddSeconds(5);
            _pendingXCurrentModuleIndex = (SubDevice.SelectedItem as ModuleInfo)?.Index ?? -1;
            _confirmedXCurrentUi = ui;
            _confirmedXCurrentModuleIndex = _pendingXCurrentModuleIndex;
            _confirmedXCurrentHoldUntil = DateTime.Now.AddMinutes(10);
            SendParamSet(ParamId.X_Cur, ui / 10);
        }

        private void button_xCurrentMinus_Click(object sender, EventArgs e)
        {
            int ui = ParseLeadingInt(textBox_xCurrent.Text);
            ui = Clamp(ui - 10, 0, 655350);
            textBox_xCurrent.Text = ui + " mA";
            _pendingXCurrentUi = ui;
            _pendingXCurrentUntil = DateTime.Now.AddSeconds(5);
            _pendingXCurrentModuleIndex = (SubDevice.SelectedItem as ModuleInfo)?.Index ?? -1;
            _confirmedXCurrentUi = ui;
            _confirmedXCurrentModuleIndex = _pendingXCurrentModuleIndex;
            _confirmedXCurrentHoldUntil = DateTime.Now.AddMinutes(10);
            SendParamSet(ParamId.X_Cur, ui / 10);
        }

        private void button_yCurrentPlus_Click(object sender, EventArgs e)
        {
            int ui = ParseLeadingInt(textBox_yCurrent.Text);
            ui = Clamp(ui + 10, 0, 655350);
            textBox_yCurrent.Text = ui + " mA";
            _pendingYCurrentUi = ui;
            _pendingYCurrentUntil = DateTime.Now.AddSeconds(5);
            _pendingYCurrentModuleIndex = (SubDevice.SelectedItem as ModuleInfo)?.Index ?? -1;
            _confirmedYCurrentUi = ui;
            _confirmedYCurrentModuleIndex = _pendingYCurrentModuleIndex;
            _confirmedYCurrentHoldUntil = DateTime.Now.AddMinutes(10);
            SendParamSet(ParamId.Y_Cur, ui / 10);
        }

        private void button_yCurrentMinus_Click(object sender, EventArgs e)
        {
            int ui = ParseLeadingInt(textBox_yCurrent.Text);
            ui = Clamp(ui - 10, 0, 655350);
            textBox_yCurrent.Text = ui + " mA";
            _pendingYCurrentUi = ui;
            _pendingYCurrentUntil = DateTime.Now.AddSeconds(5);
            _pendingYCurrentModuleIndex = (SubDevice.SelectedItem as ModuleInfo)?.Index ?? -1;
            _confirmedYCurrentUi = ui;
            _confirmedYCurrentModuleIndex = _pendingYCurrentModuleIndex;
            _confirmedYCurrentHoldUntil = DateTime.Now.AddMinutes(10);
            SendParamSet(ParamId.Y_Cur, ui / 10);
        }

        private void button_holdCurrentPlus_Click(object sender, EventArgs e)
        {
            int ui = ParseLeadingInt(textBox_holdCurrent.Text);
            int newUi = Clamp(ui + 10, 0, 100);
            textBox_holdCurrent.Text = newUi + " %";

            _pendingHoldCurrentUi = newUi;
            _pendingHoldCurrentUntil = DateTime.Now.AddSeconds(5);
            _pendingHoldCurrentModuleIndex = (SubDevice.SelectedItem as ModuleInfo)?.Index ?? -1;
            _confirmedHoldCurrentUi = newUi;
            _confirmedHoldCurrentModuleIndex = _pendingHoldCurrentModuleIndex;
            _confirmedHoldCurrentHoldUntil = DateTime.Now.AddMinutes(10);

            if (newUi == ui)
                return; // no change, do not send

            SendParamSet(ParamId.Hold, newUi);
        }

        private void button_holdCurrentMinus_Click(object sender, EventArgs e)
        {
            int ui = ParseLeadingInt(textBox_holdCurrent.Text);
            int newUi = Clamp(ui - 10, 0, 100);
            textBox_holdCurrent.Text = newUi + " %";

            _pendingHoldCurrentUi = newUi;
            _pendingHoldCurrentUntil = DateTime.Now.AddSeconds(5);
            _pendingHoldCurrentModuleIndex = (SubDevice.SelectedItem as ModuleInfo)?.Index ?? -1;
            _confirmedHoldCurrentUi = newUi;
            _confirmedHoldCurrentModuleIndex = _pendingHoldCurrentModuleIndex;
            _confirmedHoldCurrentHoldUntil = DateTime.Now.AddMinutes(10);

            if (newUi == ui)
                return; // no change, do not send

            SendParamSet(ParamId.Hold, newUi);
        }

        private void button_stepSpeedPlus_Click(object sender, EventArgs e)
        {
            int ui = ParseLeadingInt(textBox_stepSpeed.Text);
            int newUi = Clamp(ui + 100, 100, 8000);
            textBox_stepSpeed.Text = newUi.ToString();

            _pendingStepSpeedUi = newUi;
            _pendingStepSpeedUntil = DateTime.Now.AddSeconds(5);
            _pendingStepSpeedModuleIndex = (SubDevice.SelectedItem as ModuleInfo)?.Index ?? -1;
            _confirmedStepSpeedUi = newUi;
            _confirmedStepSpeedModuleIndex = _pendingStepSpeedModuleIndex;
            _confirmedStepSpeedHoldUntil = DateTime.Now.AddMinutes(10);

            if (newUi == ui)
                return; // no change, do not send

            SendParamSet(ParamId.Speed, newUi);
        }

        private void button_stepSpeedMinus_Click(object sender, EventArgs e)
        {
            int ui = ParseLeadingInt(textBox_stepSpeed.Text);
            int newUi = Clamp(ui - 100, 100, 8000);
            textBox_stepSpeed.Text = newUi.ToString();

            _pendingStepSpeedUi = newUi;
            _pendingStepSpeedUntil = DateTime.Now.AddSeconds(5);
            _pendingStepSpeedModuleIndex = (SubDevice.SelectedItem as ModuleInfo)?.Index ?? -1;
            _confirmedStepSpeedUi = newUi;
            _confirmedStepSpeedModuleIndex = _pendingStepSpeedModuleIndex;
            _confirmedStepSpeedHoldUntil = DateTime.Now.AddMinutes(10);

            if (newUi == ui)
                return; // no change, do not send

            SendParamSet(ParamId.Speed, newUi);
        }

        private void Form1_KeyUp_StepHotkeys(object sender, KeyEventArgs e)
        {
            if (TryGetDirectionButton(e.KeyCode, out var directionButton))
            {
                SetDirectionKeyPressed(e.KeyCode, directionButton, false);
                e.Handled = true;
                e.SuppressKeyPress = true;
                return;
            }

            // Only move one step on key release (no continuous repeat)
            if (e.KeyCode == Keys.Add || e.KeyCode == Keys.Oemplus)
            {
                MoveStepRadio(+1);
                e.Handled = true;
            }
            else if (e.KeyCode == Keys.Subtract || e.KeyCode == Keys.OemMinus)
            {
                MoveStepRadio(-1);
                e.Handled = true;
            }
        }

        private void Form1_KeyDown_DirectionPressedState(object sender, KeyEventArgs e)
        {
            if (!TryGetDirectionButton(e.KeyCode, out var directionButton))
                return;

            SetDirectionKeyPressed(e.KeyCode, directionButton, true);

            e.Handled = true;
            e.SuppressKeyPress = true;
        }

        private bool TryGetDirectionButton(Keys keyCode, out Button button)
        {
            button = null;
            switch (keyCode)
            {
                case Keys.Up:
                    button = button_up;
                    break;
                case Keys.Down:
                    button = button_down;
                    break;
                case Keys.Left:
                    button = button_left;
                    break;
                case Keys.Right:
                    button = button_right;
                    break;
                case Keys.Space:
                    button = button_stop;
                    break;
            }

            return button != null;
        }

        private void ReleaseAllPressedDirectionButtons()
        {
            try
            {
                _heldDirectionKeys.Clear();
                SetButtonPressedState(button_up, false, true);
                SetButtonPressedState(button_down, false, true);
                SetButtonPressedState(button_left, false, true);
                SetButtonPressedState(button_right, false, true);
                SetButtonPressedState(button_stop, false, true);
            }
            catch
            {
                // ignore
            }
        }

        private bool TryHandleDirectionKey(Keys keyCode)
        {
            if (!TryGetDirectionButton(keyCode, out var target))
                return false;

            SetDirectionKeyPressed(keyCode, target, true);

            if (target != null && target.Enabled)
            {
                try { target.PerformClick(); } catch { }
            }

            return true;
        }

        private void SetDirectionKeyPressed(Keys keyCode, Button directionButton, bool pressed)
        {
            if (directionButton == null)
                return;

            if (pressed)
            {
                if (_heldDirectionKeys.Contains(keyCode))
                    return;

                _heldDirectionKeys.Add(keyCode);
                SetButtonPressedState(directionButton, true);
            }
            else
            {
                if (_heldDirectionKeys.Remove(keyCode))
                {
                    SetButtonPressedState(directionButton, false);
                }
            }
        }

        protected override bool ProcessDialogKey(Keys keyData)
        {
            var keyCode = keyData & Keys.KeyCode;
            if (TryHandleDirectionKey(keyCode))
                return true;

            return base.ProcessDialogKey(keyData);
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            var keyCode = keyData & Keys.KeyCode;

            if (msg.Msg == WM_KEYUP || msg.Msg == WM_SYSKEYUP)
            {
                if (TryGetDirectionButton(keyCode, out var directionButton))
                {
                    SetDirectionKeyPressed(keyCode, directionButton, false);
                    return true;
                }
            }

            if (TryHandleDirectionKey(keyCode))
                return true;

            return base.ProcessCmdKey(ref msg, keyData);
        }

        private void MoveStepRadio(int delta)
        {
            var radios = GetStepRadiosOrdered();
            if (radios.Count == 0)
                return;

            int idx = radios.FindIndex(r => r.Checked);
            if (idx < 0)
                idx = delta > 0 ? 0 : radios.Count - 1;

            if (delta > 0 && idx < radios.Count - 1)
                idx++;
            else if (delta < 0 && idx > 0)
                idx--;
            else
                return;

            try { radios[idx].Checked = true; } catch { }
        }

        private List<RadioButton> GetStepRadiosOrdered()
        {
            var list = new List<RadioButton>();
            for (int i = 1; i <= 10000; i++)
            {
                var rb = this.Controls.Find($"radio_x{i}", true).FirstOrDefault() as RadioButton;
                if (rb != null)
                    list.Add(rb);
            }
            return list;
        }

        private static void ConfigureTransparentImageButton(Button button)
        {
            if (button == null)
                return;

            button.FlatStyle = FlatStyle.Flat;
            button.FlatAppearance.BorderSize = 0;
            button.FlatAppearance.MouseOverBackColor = Color.Transparent;
            button.FlatAppearance.MouseDownBackColor = Color.Transparent;
            button.BackColor = Color.Transparent;
            button.UseVisualStyleBackColor = false;
        }

        private void ConfigurePressedImageButton(Button button, string pressedImageFileName)
        {
            if (button == null || string.IsNullOrWhiteSpace(pressedImageFileName))
                return;

            var pressed = LoadImageFromImagesFolder(pressedImageFileName);
            if (pressed == null)
                return;

            _pressedButtonImages[button] = pressed;
            _normalButtonImages[button] = button.Image;
            _buttonPressRefCount[button] = 0;

            button.MouseDown += DirectionButton_MouseDown;
            button.MouseUp += DirectionButton_MouseUp;
            button.MouseLeave += DirectionButton_MouseUp;
        }

        private Image LoadImageFromImagesFolder(string fileName)
        {
            try
            {
                var path = FindImageFilePath(fileName);
                if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
                    return null;

                using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                using (var src = Image.FromStream(fs))
                {
                    return new Bitmap(src);
                }
            }
            catch
            {
                return null;
            }
        }

        private string FindImageFilePath(string fileName)
        {
            try
            {
                var baseDir = AppDomain.CurrentDomain.BaseDirectory;
                var dir = new DirectoryInfo(baseDir);
                for (int i = 0; i < 6 && dir != null; i++)
                {
                    var candidate = Path.Combine(dir.FullName, "images", fileName);
                    if (File.Exists(candidate))
                        return candidate;

                    dir = dir.Parent;
                }
            }
            catch
            {
                // ignore
            }

            return null;
        }

        private void DirectionButton_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left)
                return;

            var button = sender as Button;
            if (button == null)
                return;

            SetButtonPressedState(button, true);
        }

        private void DirectionButton_MouseUp(object sender, EventArgs e)
        {
            var button = sender as Button;
            if (button == null)
                return;

            SetButtonPressedState(button, false);
        }

        private void SetButtonPressedState(Button button, bool pressed, bool forceRelease = false)
        {
            if (button == null)
                return;

            if (!_pressedButtonImages.TryGetValue(button, out var pressedImage))
                return;

            if (!_normalButtonImages.TryGetValue(button, out var normalImage))
            {
                normalImage = button.Image;
                _normalButtonImages[button] = normalImage;
            }

            int count = _buttonPressRefCount.TryGetValue(button, out var v) ? v : 0;

            if (forceRelease)
            {
                count = 0;
            }
            else if (pressed)
            {
                count++;
            }
            else
            {
                count = Math.Max(0, count - 1);
            }

            _buttonPressRefCount[button] = count;
            button.Image = count > 0 ? pressedImage : normalImage;
        }

        private void DisposePressedButtonImages()
        {
            try
            {
                foreach (var img in _pressedButtonImages.Values)
                {
                    try { img?.Dispose(); } catch { }
                }
                _pressedButtonImages.Clear();
                _normalButtonImages.Clear();
                _buttonPressRefCount.Clear();
                _heldDirectionKeys.Clear();
            }
            catch
            {
                // ignore
            }
        }
        // =========================================================
        // OTA Firmware Upgrade over TCP (CMD_UPGRADE = 0x30)
        // =========================================================

        private const byte CMD_UPGRADE     = 0x30;
        private const byte SUBCMD_UPG_START = 0x01;
        private const byte SUBCMD_UPG_DATA  = 0x02;
        private const byte SUBCMD_UPG_END   = 0x03;
        private const byte SUBCMD_UPG_RESP  = 0x81;
        private const int  UPG_CHUNK_SIZE   = 128;

        /// <summary>
        /// Send .cic firmware to ETH device via a dedicated TCP connection.
        /// Protocol: START -> [DATA x N] -> END, request/response per packet.
        /// START may block up to ~15s while device erases W25Q sectors.
        /// </summary>
        private async Task UpgradeSendFirmwareAsync(EthDeviceItem eth, byte[] almData,
            Action<int> progressCallback)
        {
            using (var tcp = new TcpClient())
            {
                tcp.NoDelay = true;
                tcp.SendTimeout    = 10000;
                tcp.ReceiveTimeout = 60000;   // START can take ~15 s while device erases flash

                await tcp.ConnectAsync(eth.IP, eth.TcpPort);
                var stream = tcp.GetStream();

                ushort seq = 0;

                // ---- 1. UPGRADE_START ----
                if (almData.Length < 16)
                    throw new Exception("File too small to be a valid .cic firmware.");

                // START payload: file_size(4) + cic header[0..15] for device-side validation
                var startPayload = new byte[4 + 16];
                WriteLE32(startPayload, 0, (uint)almData.Length);
                Array.Copy(almData, 0, startPayload, 4, 16);
                byte[] startFrame = UpgBuildFrame(SUBCMD_UPG_START, seq, startPayload);
                await stream.WriteAsync(startFrame, 0, startFrame.Length);

                var resp = await UpgReadResp(stream);
                if (resp == null || resp[0] != 0x00)
                    throw new Exception("Upgrade failed: firmware invalid.");
                seq++;

                // ---- 2. UPGRADE_DATA chunks ----
                int offset = 0;
                int total  = almData.Length;
                while (offset < total)
                {
                    int chunkLen = Math.Min(UPG_CHUNK_SIZE, total - offset);
                    var payload  = new byte[4 + chunkLen];
                    WriteLE32(payload, 0, (uint)offset);
                    Array.Copy(almData, offset, payload, 4, chunkLen);

                    byte[] dataFrame = UpgBuildFrame(SUBCMD_UPG_DATA, seq, payload);
                    await stream.WriteAsync(dataFrame, 0, dataFrame.Length);

                    resp = await UpgReadResp(stream);
                    if (resp == null || resp[0] != 0x00)
                        throw new Exception(
                            $"Write error at offset {offset} (status=0x{(resp?[0] ?? 0xFF):X2}).");

                    offset += chunkLen;
                    seq++;

                    int pct = (int)((long)offset * 100 / total);
                    progressCallback?.Invoke(pct);
                }

                // ---- 3. UPGRADE_END ----
                var endPayload = new byte[4];
                WriteLE32(endPayload, 0, (uint)almData.Length);
                byte[] endFrame = UpgBuildFrame(SUBCMD_UPG_END, seq, endPayload);
                await stream.WriteAsync(endFrame, 0, endFrame.Length);

                resp = await UpgReadResp(stream);
                // null = device closed connection while rebooting = success
                if (resp != null && resp[0] != 0x00)
                    throw new Exception(
                        $"Device rejected END (status=0x{resp[0]:X2}). Verify failed on device.");
            }
        }

        /// <summary>
        /// Build a length-prefixed upgrade command frame.
        /// Wire format: [LEN_L][LEN_H][0xA5][0x30][0x02][SEQ_L][SEQ_H][LEN_BODY][SUBCMD][payload...]
        /// </summary>
        private static byte[] UpgBuildFrame(byte subcmd, ushort seq, byte[] payload)
        {
            int payLen   = payload?.Length ?? 0;
            int bodyLen  = 7 + payLen;           // A5+CMD+DIR+SEQ_L+SEQ_H+LEN_BODY+SUBCMD + payload
            byte lenBody = (byte)(1 + payLen);   // SUBCMD(1) + payload

            var frame = new byte[2 + bodyLen];
            frame[0] = (byte)(bodyLen & 0xFF);
            frame[1] = (byte)((bodyLen >> 8) & 0xFF);
            frame[2] = 0xA5;
            frame[3] = CMD_UPGRADE;
            frame[4] = 0x02;              // DIR_PC_TO_DEV
            frame[5] = (byte)(seq & 0xFF);
            frame[6] = (byte)(seq >> 8);
            frame[7] = lenBody;
            frame[8] = subcmd;
            if (payLen > 0)
                Array.Copy(payload, 0, frame, 9, payLen);
            return frame;
        }

        /// <summary>
        /// Read a device response frame; return the status byte array (1 byte: status).
        /// Returns null on connection error.
        /// </summary>
        private static async Task<byte[]> UpgReadResp(NetworkStream stream)
        {
            // Read 2-byte length prefix
            var lenBuf = await UpgReadExact(stream, 2);
            if (lenBuf == null) return null;

            int bodyLen = lenBuf[0] | (lenBuf[1] << 8);
            if (bodyLen < 2 || bodyLen > 256) return null;

            // Read body: [A5][CMD][DIR][SEQ_L][SEQ_H][LEN_BODY][SUBCMD][status]
            var body = await UpgReadExact(stream, bodyLen);
            if (body == null || body.Length < 8) return null;

            // Validate frame header
            if (body[0] != 0xA5 || body[1] != CMD_UPGRADE || body[6] != SUBCMD_UPG_RESP)
                return null;

            return new[] { body[7] }; // status byte
        }

        private static async Task<byte[]> UpgReadExact(NetworkStream stream, int count)
        {
            var buf  = new byte[count];
            int read = 0;
            while (read < count)
            {
                int n;
                try { n = await stream.ReadAsync(buf, read, count - read); }
                catch { return null; }
                if (n == 0) return null;
                read += n;
            }
            return buf;
        }

        private static void WriteLE32(byte[] buf, int offset, uint value)
        {
            buf[offset]     = (byte)(value & 0xFF);
            buf[offset + 1] = (byte)((value >> 8) & 0xFF);
            buf[offset + 2] = (byte)((value >> 16) & 0xFF);
            buf[offset + 3] = (byte)((value >> 24) & 0xFF);
        }

        // -----------------------------------------------------------------
        //  Module OTA upgrade (CMD_MODULE_UPGRADE = 0x31) — relay .mot to a
        //  sub-module via the device's CAN-FD bus. Mirrors the device-side
        //  staging at W25Q[0x100000] and the per-frame ACK protocol.
        // -----------------------------------------------------------------

        private const byte CMD_MODULE_UPGRADE = 0x31;
        private const byte SUBCMD_MUPG_START  = 0x01;
        private const byte SUBCMD_MUPG_DATA   = 0x02;
        private const byte SUBCMD_MUPG_END    = 0x03;
        private const byte SUBCMD_MUPG_RESP   = 0x81;
        private const int  MUPG_CHUNK_SIZE    = 128;

        /// <summary>
        /// Send .mot firmware to a sub-module via the ETH device (relay).
        /// Protocol: START -> [DATA x N] -> END, request/response per packet.
        /// START erases ~ceil(filesize, 4 KB) of W25Q on the device, so it
        /// can take up to a few seconds for large firmwares.
        /// </summary>
        private async Task ModuleUpgradeSendFirmwareAsync(EthDeviceItem eth,
            uint targetNodeId, byte[] motData, Action<int> progressCallback)
        {
            using (var tcp = new TcpClient())
            {
                tcp.NoDelay        = true;
                tcp.SendTimeout    = 10000;
                tcp.ReceiveTimeout = 60000;

                await tcp.ConnectAsync(eth.IP, eth.TcpPort);
                var stream = tcp.GetStream();

                ushort seq = 0;

                // ---- 1. MUPG_START ----
                // payload: file_size(4) + target_node_id(4) + cic_hdr_16(16) = 24
                var startPayload = new byte[4 + 4 + 16];
                WriteLE32(startPayload, 0, (uint)motData.Length);
                WriteLE32(startPayload, 4, targetNodeId);
                Array.Copy(motData, 0, startPayload, 8, 16);
                byte[] startFrame = MupgBuildFrame(SUBCMD_MUPG_START, seq, startPayload);
                await stream.WriteAsync(startFrame, 0, startFrame.Length);

                var resp = await MupgReadResp(stream);
                if (resp == null || resp[0] != 0x00)
                    throw new Exception(
                        $"START rejected (status=0x{(resp?[0] ?? 0xFF):X2}).");
                seq++;

                // ---- 2. MUPG_DATA chunks ----
                int offset = 0;
                int total  = motData.Length;
                while (offset < total)
                {
                    int chunkLen = Math.Min(MUPG_CHUNK_SIZE, total - offset);
                    var payload  = new byte[4 + chunkLen];
                    WriteLE32(payload, 0, (uint)offset);
                    Array.Copy(motData, offset, payload, 4, chunkLen);

                    byte[] dataFrame = MupgBuildFrame(SUBCMD_MUPG_DATA, seq, payload);
                    await stream.WriteAsync(dataFrame, 0, dataFrame.Length);

                    resp = await MupgReadResp(stream);
                    if (resp == null || resp[0] != 0x00)
                        throw new Exception(
                            $"Write error at offset {offset} (status=0x{(resp?[0] ?? 0xFF):X2}).");

                    offset += chunkLen;
                    seq++;

                    int pct = (int)((long)offset * 100 / total);
                    progressCallback?.Invoke(pct);
                }

                // ---- 3. MUPG_END ----
                var endPayload = new byte[4];
                WriteLE32(endPayload, 0, (uint)motData.Length);
                byte[] endFrame = MupgBuildFrame(SUBCMD_MUPG_END, seq, endPayload);
                await stream.WriteAsync(endFrame, 0, endFrame.Length);

                resp = await MupgReadResp(stream);
                if (resp != null && resp[0] != 0x00)
                    throw new Exception(
                        $"Device rejected END (status=0x{resp[0]:X2}).");
                // After END_OK the device has staged .mot and starts CAN-FD
                // relay asynchronously. The TCP connection is no longer needed.
            }
        }

        private static byte[] MupgBuildFrame(byte subcmd, ushort seq, byte[] payload)
        {
            int payLen   = payload?.Length ?? 0;
            int bodyLen  = 7 + payLen;
            byte lenBody = (byte)(1 + payLen);

            var frame = new byte[2 + bodyLen];
            frame[0] = (byte)(bodyLen & 0xFF);
            frame[1] = (byte)((bodyLen >> 8) & 0xFF);
            frame[2] = 0xA5;
            frame[3] = CMD_MODULE_UPGRADE;
            frame[4] = 0x02;              // DIR_PC_TO_DEV
            frame[5] = (byte)(seq & 0xFF);
            frame[6] = (byte)(seq >> 8);
            frame[7] = lenBody;
            frame[8] = subcmd;
            if (payLen > 0)
                Array.Copy(payload, 0, frame, 9, payLen);
            return frame;
        }

        private static async Task<byte[]> MupgReadResp(NetworkStream stream)
        {
            var lenBuf = await UpgReadExact(stream, 2);
            if (lenBuf == null) return null;

            int bodyLen = lenBuf[0] | (lenBuf[1] << 8);
            if (bodyLen < 2 || bodyLen > 256) return null;

            var body = await UpgReadExact(stream, bodyLen);
            if (body == null || body.Length < 8) return null;

            if (body[0] != 0xA5 || body[1] != CMD_MODULE_UPGRADE || body[6] != SUBCMD_MUPG_RESP)
                return null;

            return new[] { body[7] };
        }

    }
}

