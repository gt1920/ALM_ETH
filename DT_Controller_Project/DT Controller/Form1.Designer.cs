namespace DT_Controller
{
    partial class Form1
    {
        /// <summary>
        /// 必需的设计器变量。
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// 清理所有正在使用的资源。
        /// </summary>
        /// <param name="disposing">如果应释放托管资源，为 true；否则为 false。</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows 窗体设计器生成的代码

        /// <summary>
        /// 设计器支持所需的方法 - 不要修改
        /// 使用代码编辑器修改此方法的内容。
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.MainDevice = new System.Windows.Forms.ListBox();
            this.SubDevice = new System.Windows.Forms.ListBox();
            this.button_up = new System.Windows.Forms.Button();
            this.radio_x1 = new System.Windows.Forms.RadioButton();
            this.radio_x10 = new System.Windows.Forms.RadioButton();
            this.radio_x100 = new System.Windows.Forms.RadioButton();
            this.button_left = new System.Windows.Forms.Button();
            this.button_right = new System.Windows.Forms.Button();
            this.button_down = new System.Windows.Forms.Button();
            this.radio_x1000 = new System.Windows.Forms.RadioButton();
            this.button5 = new System.Windows.Forms.Button();
            this.button7 = new System.Windows.Forms.Button();
            this.button8 = new System.Windows.Forms.Button();
            this.richTextBox1 = new System.Windows.Forms.RichTextBox();
            this.remain = new System.Windows.Forms.Label();
            this.button10 = new System.Windows.Forms.Button();
            this.label4 = new System.Windows.Forms.Label();
            this.label6 = new System.Windows.Forms.Label();
            this.Device_Info = new System.Windows.Forms.RichTextBox();
            this.label7 = new System.Windows.Forms.Label();
            this.Module_Info_richTextBox = new System.Windows.Forms.RichTextBox();
            this.label8 = new System.Windows.Forms.Label();
            this.radio_x2000 = new System.Windows.Forms.RadioButton();
            this.radio_x3 = new System.Windows.Forms.RadioButton();
            this.radio_x5 = new System.Windows.Forms.RadioButton();
            this.label_xCurrent = new System.Windows.Forms.Label();
            this.textBox_xCurrent = new System.Windows.Forms.TextBox();
            this.button_xCurrentMinus = new System.Windows.Forms.Button();
            this.button_xCurrentPlus = new System.Windows.Forms.Button();
            this.label_yCurrent = new System.Windows.Forms.Label();
            this.textBox_yCurrent = new System.Windows.Forms.TextBox();
            this.button_yCurrentMinus = new System.Windows.Forms.Button();
            this.button_yCurrentPlus = new System.Windows.Forms.Button();
            this.label_holdCurrent = new System.Windows.Forms.Label();
            this.textBox_holdCurrent = new System.Windows.Forms.TextBox();
            this.button_holdCurrentMinus = new System.Windows.Forms.Button();
            this.button_holdCurrentPlus = new System.Windows.Forms.Button();
            this.label_stepSpeed = new System.Windows.Forms.Label();
            this.textBox_stepSpeed = new System.Windows.Forms.TextBox();
            this.button_stepSpeedMinus = new System.Windows.Forms.Button();
            this.button_stepSpeedPlus = new System.Windows.Forms.Button();
            this.checkBoxR = new System.Windows.Forms.CheckBox();
            this.checkBoxG = new System.Windows.Forms.CheckBox();
            this.checkBoxB = new System.Windows.Forms.CheckBox();
            this.checkBoxXY = new System.Windows.Forms.CheckBox();
            this.checkBoxX = new System.Windows.Forms.CheckBox();
            this.checkBoxY = new System.Windows.Forms.CheckBox();
            this.button_stop = new System.Windows.Forms.Button();
            this.label_pos = new System.Windows.Forms.Label();
            this.button_clearLog = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // MainDevice
            // 
            this.MainDevice.Font = new System.Drawing.Font("Microsoft Sans Serif", 9F);
            this.MainDevice.FormattingEnabled = true;
            this.MainDevice.ItemHeight = 22;
            this.MainDevice.Location = new System.Drawing.Point(14, 76);
            this.MainDevice.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.MainDevice.Name = "MainDevice";
            this.MainDevice.Size = new System.Drawing.Size(120, 224);
            this.MainDevice.TabIndex = 0;
            this.MainDevice.SelectedIndexChanged += new System.EventHandler(this.MainDevice_SelectedIndexChanged);
            // 
            // SubDevice
            // 
            this.SubDevice.FormattingEnabled = true;
            this.SubDevice.ItemHeight = 22;
            this.SubDevice.Location = new System.Drawing.Point(140, 76);
            this.SubDevice.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.SubDevice.Name = "SubDevice";
            this.SubDevice.Size = new System.Drawing.Size(120, 224);
            this.SubDevice.TabIndex = 1;
            // 
            // button_up
            // 
            this.button_up.FlatAppearance.BorderSize = 0;
            this.button_up.Image = ((System.Drawing.Image)(resources.GetObject("button_up.Image")));
            this.button_up.Location = new System.Drawing.Point(350, 80);
            this.button_up.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.button_up.Name = "button_up";
            this.button_up.Size = new System.Drawing.Size(60, 60);
            this.button_up.TabIndex = 2;
            this.button_up.UseVisualStyleBackColor = false;
            this.button_up.Click += new System.EventHandler(this.button_up_Click);
            // 
            // radio_x1
            // 
            this.radio_x1.AutoSize = true;
            this.radio_x1.Checked = true;
            this.radio_x1.Location = new System.Drawing.Point(718, 52);
            this.radio_x1.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radio_x1.Name = "radio_x1";
            this.radio_x1.Size = new System.Drawing.Size(54, 26);
            this.radio_x1.TabIndex = 3;
            this.radio_x1.TabStop = true;
            this.radio_x1.Text = "x1";
            this.radio_x1.UseVisualStyleBackColor = true;
            // 
            // radio_x10
            // 
            this.radio_x10.AutoSize = true;
            this.radio_x10.Location = new System.Drawing.Point(718, 133);
            this.radio_x10.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radio_x10.Name = "radio_x10";
            this.radio_x10.Size = new System.Drawing.Size(64, 26);
            this.radio_x10.TabIndex = 4;
            this.radio_x10.Text = "x10";
            this.radio_x10.UseVisualStyleBackColor = true;
            // 
            // radio_x100
            // 
            this.radio_x100.AutoSize = true;
            this.radio_x100.Location = new System.Drawing.Point(718, 160);
            this.radio_x100.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radio_x100.Name = "radio_x100";
            this.radio_x100.Size = new System.Drawing.Size(74, 26);
            this.radio_x100.TabIndex = 5;
            this.radio_x100.Text = "x100";
            this.radio_x100.UseVisualStyleBackColor = true;
            // 
            // button_left
            // 
            this.button_left.Image = ((System.Drawing.Image)(resources.GetObject("button_left.Image")));
            this.button_left.Location = new System.Drawing.Point(280, 150);
            this.button_left.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.button_left.Name = "button_left";
            this.button_left.Size = new System.Drawing.Size(60, 60);
            this.button_left.TabIndex = 6;
            this.button_left.UseVisualStyleBackColor = true;
            this.button_left.Click += new System.EventHandler(this.button_left_Click);
            // 
            // button_right
            // 
            this.button_right.Image = ((System.Drawing.Image)(resources.GetObject("button_right.Image")));
            this.button_right.Location = new System.Drawing.Point(420, 150);
            this.button_right.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.button_right.Name = "button_right";
            this.button_right.Size = new System.Drawing.Size(60, 60);
            this.button_right.TabIndex = 7;
            this.button_right.UseVisualStyleBackColor = true;
            this.button_right.Click += new System.EventHandler(this.button_right_Click);
            // 
            // button_down
            // 
            this.button_down.Image = ((System.Drawing.Image)(resources.GetObject("button_down.Image")));
            this.button_down.Location = new System.Drawing.Point(350, 220);
            this.button_down.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.button_down.Name = "button_down";
            this.button_down.Size = new System.Drawing.Size(60, 60);
            this.button_down.TabIndex = 8;
            this.button_down.UseVisualStyleBackColor = true;
            this.button_down.Click += new System.EventHandler(this.button_down_Click);
            // 
            // radio_x1000
            // 
            this.radio_x1000.AutoSize = true;
            this.radio_x1000.Location = new System.Drawing.Point(718, 187);
            this.radio_x1000.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radio_x1000.Name = "radio_x1000";
            this.radio_x1000.Size = new System.Drawing.Size(84, 26);
            this.radio_x1000.TabIndex = 9;
            this.radio_x1000.Text = "x1000";
            this.radio_x1000.UseVisualStyleBackColor = true;
            // 
            // button5
            // 
            this.button5.Location = new System.Drawing.Point(299, 400);
            this.button5.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.button5.Name = "button5";
            this.button5.Size = new System.Drawing.Size(80, 60);
            this.button5.TabIndex = 10;
            this.button5.Text = "Identify";
            this.button5.UseVisualStyleBackColor = true;
            // 
            // button7
            // 
            this.button7.FlatAppearance.BorderSize = 0;
            this.button7.Image = ((System.Drawing.Image)(resources.GetObject("button7.Image")));
            this.button7.Location = new System.Drawing.Point(299, 325);
            this.button7.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.button7.Name = "button7";
            this.button7.Size = new System.Drawing.Size(60, 60);
            this.button7.TabIndex = 16;
            this.button7.UseVisualStyleBackColor = true;
            // 
            // button8
            // 
            this.button8.Image = ((System.Drawing.Image)(resources.GetObject("button8.Image")));
            this.button8.Location = new System.Drawing.Point(396, 325);
            this.button8.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.button8.Name = "button8";
            this.button8.Size = new System.Drawing.Size(60, 60);
            this.button8.TabIndex = 17;
            this.button8.UseVisualStyleBackColor = true;
            // 
            // richTextBox1
            // 
            this.richTextBox1.Location = new System.Drawing.Point(14, 496);
            this.richTextBox1.Name = "richTextBox1";
            this.richTextBox1.Size = new System.Drawing.Size(810, 213);
            this.richTextBox1.TabIndex = 19;
            this.richTextBox1.Text = "";
            // 
            // remain
            // 
            this.remain.AutoSize = true;
            this.remain.ForeColor = System.Drawing.SystemColors.GrayText;
            this.remain.Location = new System.Drawing.Point(500, 186);
            this.remain.Name = "remain";
            this.remain.Size = new System.Drawing.Size(89, 22);
            this.remain.TabIndex = 20;
            this.remain.Text = "X: 0 / Y: 0";
            this.remain.Click += new System.EventHandler(this.label3_Click);
            // 
            // button10
            // 
            this.button10.Location = new System.Drawing.Point(396, 400);
            this.button10.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.button10.Name = "button10";
            this.button10.Size = new System.Drawing.Size(80, 60);
            this.button10.TabIndex = 21;
            this.button10.Text = "Test";
            this.button10.UseVisualStyleBackColor = true;
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(14, 52);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(98, 22);
            this.label4.TabIndex = 25;
            this.label4.Text = "Device List";
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(136, 52);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(101, 22);
            this.label6.TabIndex = 26;
            this.label6.Text = "Module List";
            // 
            // Device_Info
            // 
            this.Device_Info.BackColor = System.Drawing.SystemColors.ControlLight;
            this.Device_Info.Location = new System.Drawing.Point(13, 382);
            this.Device_Info.Name = "Device_Info";
            this.Device_Info.Size = new System.Drawing.Size(120, 100);
            this.Device_Info.TabIndex = 27;
            this.Device_Info.Text = "";
            this.Device_Info.TextChanged += new System.EventHandler(this.Device_Info_TextChanged);
            // 
            // label7
            // 
            this.label7.AutoSize = true;
            this.label7.Location = new System.Drawing.Point(14, 361);
            this.label7.Name = "label7";
            this.label7.Size = new System.Drawing.Size(99, 22);
            this.label7.TabIndex = 28;
            this.label7.Text = "Device Info";
            // 
            // Module_Info_richTextBox
            // 
            this.Module_Info_richTextBox.BackColor = System.Drawing.SystemColors.ControlLight;
            this.Module_Info_richTextBox.Location = new System.Drawing.Point(140, 382);
            this.Module_Info_richTextBox.Name = "Module_Info_richTextBox";
            this.Module_Info_richTextBox.Size = new System.Drawing.Size(120, 100);
            this.Module_Info_richTextBox.TabIndex = 29;
            this.Module_Info_richTextBox.Text = "";
            // 
            // label8
            // 
            this.label8.AutoSize = true;
            this.label8.Location = new System.Drawing.Point(137, 361);
            this.label8.Name = "label8";
            this.label8.Size = new System.Drawing.Size(102, 22);
            this.label8.TabIndex = 30;
            this.label8.Text = "Module Info";
            // 
            // radio_x2000
            // 
            this.radio_x2000.AutoSize = true;
            this.radio_x2000.Location = new System.Drawing.Point(718, 214);
            this.radio_x2000.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radio_x2000.Name = "radio_x2000";
            this.radio_x2000.Size = new System.Drawing.Size(84, 26);
            this.radio_x2000.TabIndex = 31;
            this.radio_x2000.Text = "x2000";
            this.radio_x2000.UseVisualStyleBackColor = true;
            this.radio_x2000.CheckedChanged += new System.EventHandler(this.radioButton4_CheckedChanged);
            // 
            // radio_x3
            // 
            this.radio_x3.AutoSize = true;
            this.radio_x3.Location = new System.Drawing.Point(718, 79);
            this.radio_x3.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radio_x3.Name = "radio_x3";
            this.radio_x3.Size = new System.Drawing.Size(54, 26);
            this.radio_x3.TabIndex = 32;
            this.radio_x3.TabStop = true;
            this.radio_x3.Text = "x3";
            this.radio_x3.UseVisualStyleBackColor = true;
            // 
            // radio_x5
            // 
            this.radio_x5.AutoSize = true;
            this.radio_x5.Location = new System.Drawing.Point(718, 106);
            this.radio_x5.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radio_x5.Name = "radio_x5";
            this.radio_x5.Size = new System.Drawing.Size(54, 26);
            this.radio_x5.TabIndex = 33;
            this.radio_x5.Text = "x5";
            this.radio_x5.UseVisualStyleBackColor = true;
            // 
            // label_xCurrent
            // 
            this.label_xCurrent.AutoSize = true;
            this.label_xCurrent.Location = new System.Drawing.Point(718, 245);
            this.label_xCurrent.Name = "label_xCurrent";
            this.label_xCurrent.Size = new System.Drawing.Size(56, 22);
            this.label_xCurrent.TabIndex = 34;
            this.label_xCurrent.Text = "X Cur";
            // 
            // textBox_xCurrent
            // 
            this.textBox_xCurrent.Location = new System.Drawing.Point(718, 265);
            this.textBox_xCurrent.Name = "textBox_xCurrent";
            this.textBox_xCurrent.ReadOnly = true;
            this.textBox_xCurrent.Size = new System.Drawing.Size(66, 28);
            this.textBox_xCurrent.TabIndex = 35;
            this.textBox_xCurrent.Text = "250 mA";
            this.textBox_xCurrent.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
            // 
            // button_xCurrentMinus
            // 
            this.button_xCurrentMinus.Location = new System.Drawing.Point(790, 265);
            this.button_xCurrentMinus.Name = "button_xCurrentMinus";
            this.button_xCurrentMinus.Size = new System.Drawing.Size(28, 24);
            this.button_xCurrentMinus.TabIndex = 36;
            this.button_xCurrentMinus.Text = "-";
            this.button_xCurrentMinus.UseVisualStyleBackColor = true;
            // 
            // button_xCurrentPlus
            // 
            this.button_xCurrentPlus.Location = new System.Drawing.Point(824, 265);
            this.button_xCurrentPlus.Name = "button_xCurrentPlus";
            this.button_xCurrentPlus.Size = new System.Drawing.Size(28, 24);
            this.button_xCurrentPlus.TabIndex = 37;
            this.button_xCurrentPlus.Text = "+";
            this.button_xCurrentPlus.UseVisualStyleBackColor = true;
            // 
            // label_yCurrent
            // 
            this.label_yCurrent.AutoSize = true;
            this.label_yCurrent.Location = new System.Drawing.Point(718, 296);
            this.label_yCurrent.Name = "label_yCurrent";
            this.label_yCurrent.Size = new System.Drawing.Size(56, 22);
            this.label_yCurrent.TabIndex = 38;
            this.label_yCurrent.Text = "Y Cur";
            // 
            // textBox_yCurrent
            // 
            this.textBox_yCurrent.Location = new System.Drawing.Point(718, 316);
            this.textBox_yCurrent.Name = "textBox_yCurrent";
            this.textBox_yCurrent.ReadOnly = true;
            this.textBox_yCurrent.Size = new System.Drawing.Size(66, 28);
            this.textBox_yCurrent.TabIndex = 39;
            this.textBox_yCurrent.Text = "250 mA";
            this.textBox_yCurrent.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
            // 
            // button_yCurrentMinus
            // 
            this.button_yCurrentMinus.Location = new System.Drawing.Point(790, 316);
            this.button_yCurrentMinus.Name = "button_yCurrentMinus";
            this.button_yCurrentMinus.Size = new System.Drawing.Size(28, 24);
            this.button_yCurrentMinus.TabIndex = 40;
            this.button_yCurrentMinus.Text = "-";
            this.button_yCurrentMinus.UseVisualStyleBackColor = true;
            // 
            // button_yCurrentPlus
            // 
            this.button_yCurrentPlus.Location = new System.Drawing.Point(824, 316);
            this.button_yCurrentPlus.Name = "button_yCurrentPlus";
            this.button_yCurrentPlus.Size = new System.Drawing.Size(28, 24);
            this.button_yCurrentPlus.TabIndex = 41;
            this.button_yCurrentPlus.Text = "+";
            this.button_yCurrentPlus.UseVisualStyleBackColor = true;
            // 
            // label_holdCurrent
            // 
            this.label_holdCurrent.AutoSize = true;
            this.label_holdCurrent.Location = new System.Drawing.Point(718, 347);
            this.label_holdCurrent.Name = "label_holdCurrent";
            this.label_holdCurrent.Size = new System.Drawing.Size(81, 22);
            this.label_holdCurrent.TabIndex = 42;
            this.label_holdCurrent.Text = "Hold Cur";
            // 
            // textBox_holdCurrent
            // 
            this.textBox_holdCurrent.Location = new System.Drawing.Point(718, 367);
            this.textBox_holdCurrent.Name = "textBox_holdCurrent";
            this.textBox_holdCurrent.ReadOnly = true;
            this.textBox_holdCurrent.Size = new System.Drawing.Size(66, 28);
            this.textBox_holdCurrent.TabIndex = 43;
            this.textBox_holdCurrent.Text = "0 %";
            this.textBox_holdCurrent.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
            // 
            // button_holdCurrentMinus
            // 
            this.button_holdCurrentMinus.Location = new System.Drawing.Point(790, 367);
            this.button_holdCurrentMinus.Name = "button_holdCurrentMinus";
            this.button_holdCurrentMinus.Size = new System.Drawing.Size(28, 24);
            this.button_holdCurrentMinus.TabIndex = 44;
            this.button_holdCurrentMinus.Text = "-";
            this.button_holdCurrentMinus.UseVisualStyleBackColor = true;
            // 
            // button_holdCurrentPlus
            // 
            this.button_holdCurrentPlus.Location = new System.Drawing.Point(824, 367);
            this.button_holdCurrentPlus.Name = "button_holdCurrentPlus";
            this.button_holdCurrentPlus.Size = new System.Drawing.Size(28, 24);
            this.button_holdCurrentPlus.TabIndex = 45;
            this.button_holdCurrentPlus.Text = "+";
            this.button_holdCurrentPlus.UseVisualStyleBackColor = true;
            // 
            // label_stepSpeed
            // 
            this.label_stepSpeed.AutoSize = true;
            this.label_stepSpeed.Location = new System.Drawing.Point(718, 398);
            this.label_stepSpeed.Name = "label_stepSpeed";
            this.label_stepSpeed.Size = new System.Drawing.Size(61, 22);
            this.label_stepSpeed.TabIndex = 46;
            this.label_stepSpeed.Text = "Step/s";
            // 
            // textBox_stepSpeed
            // 
            this.textBox_stepSpeed.Location = new System.Drawing.Point(718, 418);
            this.textBox_stepSpeed.Name = "textBox_stepSpeed";
            this.textBox_stepSpeed.ReadOnly = true;
            this.textBox_stepSpeed.Size = new System.Drawing.Size(66, 28);
            this.textBox_stepSpeed.TabIndex = 47;
            this.textBox_stepSpeed.Text = "250";
            this.textBox_stepSpeed.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
            // 
            // button_stepSpeedMinus
            // 
            this.button_stepSpeedMinus.Location = new System.Drawing.Point(790, 418);
            this.button_stepSpeedMinus.Name = "button_stepSpeedMinus";
            this.button_stepSpeedMinus.Size = new System.Drawing.Size(28, 24);
            this.button_stepSpeedMinus.TabIndex = 48;
            this.button_stepSpeedMinus.Text = "-";
            this.button_stepSpeedMinus.UseVisualStyleBackColor = true;
            // 
            // button_stepSpeedPlus
            // 
            this.button_stepSpeedPlus.Location = new System.Drawing.Point(824, 418);
            this.button_stepSpeedPlus.Name = "button_stepSpeedPlus";
            this.button_stepSpeedPlus.Size = new System.Drawing.Size(28, 24);
            this.button_stepSpeedPlus.TabIndex = 49;
            this.button_stepSpeedPlus.Text = "+";
            this.button_stepSpeedPlus.UseVisualStyleBackColor = true;
            // 
            // checkBoxR
            // 
            this.checkBoxR.AutoSize = true;
            this.checkBoxR.Checked = true;
            this.checkBoxR.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxR.Location = new System.Drawing.Point(530, 53);
            this.checkBoxR.Name = "checkBoxR";
            this.checkBoxR.Size = new System.Drawing.Size(49, 26);
            this.checkBoxR.TabIndex = 50;
            this.checkBoxR.Text = "R";
            this.checkBoxR.UseVisualStyleBackColor = true;
            // 
            // checkBoxG
            // 
            this.checkBoxG.AutoSize = true;
            this.checkBoxG.Checked = true;
            this.checkBoxG.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxG.Location = new System.Drawing.Point(580, 53);
            this.checkBoxG.Name = "checkBoxG";
            this.checkBoxG.Size = new System.Drawing.Size(50, 26);
            this.checkBoxG.TabIndex = 51;
            this.checkBoxG.Text = "G";
            this.checkBoxG.UseVisualStyleBackColor = true;
            // 
            // checkBoxB
            // 
            this.checkBoxB.AutoSize = true;
            this.checkBoxB.Checked = true;
            this.checkBoxB.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxB.Location = new System.Drawing.Point(630, 53);
            this.checkBoxB.Name = "checkBoxB";
            this.checkBoxB.Size = new System.Drawing.Size(48, 26);
            this.checkBoxB.TabIndex = 52;
            this.checkBoxB.Text = "B";
            this.checkBoxB.UseVisualStyleBackColor = true;
            // 
            // checkBoxXY
            // 
            this.checkBoxXY.AutoSize = true;
            this.checkBoxXY.Location = new System.Drawing.Point(530, 81);
            this.checkBoxXY.Name = "checkBoxXY";
            this.checkBoxXY.Size = new System.Drawing.Size(65, 26);
            this.checkBoxXY.TabIndex = 53;
            this.checkBoxXY.Text = "X/Y";
            this.checkBoxXY.UseVisualStyleBackColor = true;
            // 
            // checkBoxX
            // 
            this.checkBoxX.AutoSize = true;
            this.checkBoxX.Location = new System.Drawing.Point(580, 81);
            this.checkBoxX.Name = "checkBoxX";
            this.checkBoxX.Size = new System.Drawing.Size(48, 26);
            this.checkBoxX.TabIndex = 54;
            this.checkBoxX.Text = "X";
            this.checkBoxX.UseVisualStyleBackColor = true;
            // 
            // checkBoxY
            // 
            this.checkBoxY.AutoSize = true;
            this.checkBoxY.Location = new System.Drawing.Point(630, 81);
            this.checkBoxY.Name = "checkBoxY";
            this.checkBoxY.Size = new System.Drawing.Size(48, 26);
            this.checkBoxY.TabIndex = 55;
            this.checkBoxY.Text = "Y";
            this.checkBoxY.UseVisualStyleBackColor = true;
            // 
            // button_stop
            // 
            this.button_stop.FlatAppearance.BorderSize = 0;
            this.button_stop.Image = ((System.Drawing.Image)(resources.GetObject("button_stop.Image")));
            this.button_stop.Location = new System.Drawing.Point(340, 140);
            this.button_stop.Name = "button_stop";
            this.button_stop.Size = new System.Drawing.Size(80, 80);
            this.button_stop.TabIndex = 56;
            this.button_stop.UseVisualStyleBackColor = true;
            // 
            // label_pos
            // 
            this.label_pos.AutoSize = true;
            this.label_pos.Location = new System.Drawing.Point(500, 160);
            this.label_pos.Name = "label_pos";
            this.label_pos.Size = new System.Drawing.Size(175, 22);
            this.label_pos.TabIndex = 57;
            this.label_pos.Text = "Pos  X:0000  Y:0000";
            // 
            // button_clearLog
            // 
            this.button_clearLog.Location = new System.Drawing.Point(830, 496);
            this.button_clearLog.Name = "button_clearLog";
            this.button_clearLog.Size = new System.Drawing.Size(110, 32);
            this.button_clearLog.TabIndex = 58;
            this.button_clearLog.Text = "Clear Log";
            this.button_clearLog.UseVisualStyleBackColor = true;
            this.button_clearLog.Click += new System.EventHandler(this.button_clearLog_Click);
            //
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(10F, 22F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1006, 721);
            this.Controls.Add(this.button_clearLog);
            this.Controls.Add(this.label_pos);
            this.Controls.Add(this.button_stop);
            this.Controls.Add(this.checkBoxY);
            this.Controls.Add(this.checkBoxX);
            this.Controls.Add(this.checkBoxXY);
            this.Controls.Add(this.checkBoxB);
            this.Controls.Add(this.checkBoxG);
            this.Controls.Add(this.checkBoxR);
            this.Controls.Add(this.button_stepSpeedPlus);
            this.Controls.Add(this.button_stepSpeedMinus);
            this.Controls.Add(this.textBox_stepSpeed);
            this.Controls.Add(this.label_stepSpeed);
            this.Controls.Add(this.button_holdCurrentPlus);
            this.Controls.Add(this.button_holdCurrentMinus);
            this.Controls.Add(this.textBox_holdCurrent);
            this.Controls.Add(this.label_holdCurrent);
            this.Controls.Add(this.button_yCurrentPlus);
            this.Controls.Add(this.button_yCurrentMinus);
            this.Controls.Add(this.textBox_yCurrent);
            this.Controls.Add(this.label_yCurrent);
            this.Controls.Add(this.button_xCurrentPlus);
            this.Controls.Add(this.button_xCurrentMinus);
            this.Controls.Add(this.textBox_xCurrent);
            this.Controls.Add(this.label_xCurrent);
            this.Controls.Add(this.radio_x5);
            this.Controls.Add(this.radio_x3);
            this.Controls.Add(this.radio_x2000);
            this.Controls.Add(this.label8);
            this.Controls.Add(this.Module_Info_richTextBox);
            this.Controls.Add(this.label7);
            this.Controls.Add(this.Device_Info);
            this.Controls.Add(this.label6);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.button10);
            this.Controls.Add(this.remain);
            this.Controls.Add(this.richTextBox1);
            this.Controls.Add(this.button8);
            this.Controls.Add(this.button7);
            this.Controls.Add(this.button5);
            this.Controls.Add(this.radio_x1000);
            this.Controls.Add(this.button_down);
            this.Controls.Add(this.button_right);
            this.Controls.Add(this.button_left);
            this.Controls.Add(this.radio_x100);
            this.Controls.Add(this.radio_x10);
            this.Controls.Add(this.radio_x1);
            this.Controls.Add(this.button_up);
            this.Controls.Add(this.SubDevice);
            this.Controls.Add(this.MainDevice);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.MaximizeBox = false;
            this.Name = "Form1";
            this.Text = "DT Controller";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ListBox MainDevice;
        private System.Windows.Forms.ListBox SubDevice;
        private System.Windows.Forms.Button button_up;
        private System.Windows.Forms.RadioButton radio_x1;
        private System.Windows.Forms.RadioButton radio_x10;
        private System.Windows.Forms.RadioButton radio_x100;
        private System.Windows.Forms.Button button_left;
        private System.Windows.Forms.Button button_right;
        private System.Windows.Forms.Button button_down;
        private System.Windows.Forms.RadioButton radio_x1000;
        private System.Windows.Forms.Button button5;
        private System.Windows.Forms.Button button7;
        private System.Windows.Forms.Button button8;
        private System.Windows.Forms.RichTextBox richTextBox1;
        private System.Windows.Forms.Label remain;
        private System.Windows.Forms.Button button10;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.RichTextBox Device_Info;
        private System.Windows.Forms.Label label7;
        private System.Windows.Forms.RichTextBox Module_Info_richTextBox;
        private System.Windows.Forms.Label label8;
        private System.Windows.Forms.RadioButton radio_x2000;
        private System.Windows.Forms.RadioButton radio_x3;
        private System.Windows.Forms.RadioButton radio_x5;
        private System.Windows.Forms.Label label_xCurrent;
        private System.Windows.Forms.TextBox textBox_xCurrent;
        private System.Windows.Forms.Button button_xCurrentMinus;
        private System.Windows.Forms.Button button_xCurrentPlus;
        private System.Windows.Forms.Label label_yCurrent;
        private System.Windows.Forms.TextBox textBox_yCurrent;
        private System.Windows.Forms.Button button_yCurrentMinus;
        private System.Windows.Forms.Button button_yCurrentPlus;
        private System.Windows.Forms.Label label_holdCurrent;
        private System.Windows.Forms.TextBox textBox_holdCurrent;
        private System.Windows.Forms.Button button_holdCurrentMinus;
        private System.Windows.Forms.Button button_holdCurrentPlus;
        private System.Windows.Forms.Label label_stepSpeed;
        private System.Windows.Forms.TextBox textBox_stepSpeed;
        private System.Windows.Forms.Button button_stepSpeedMinus;
        private System.Windows.Forms.Button button_stepSpeedPlus;
        private System.Windows.Forms.CheckBox checkBoxR;
        private System.Windows.Forms.CheckBox checkBoxG;
        private System.Windows.Forms.CheckBox checkBoxB;
        private System.Windows.Forms.CheckBox checkBoxXY;
        private System.Windows.Forms.CheckBox checkBoxX;
        private System.Windows.Forms.CheckBox checkBoxY;
        private System.Windows.Forms.Button button_stop;
        private System.Windows.Forms.Label label_pos;
        private System.Windows.Forms.Button button_clearLog;
    }
}

