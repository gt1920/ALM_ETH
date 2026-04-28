当前接口已经是你在回调里唯一要调用的函数：
AdjusterCAN_OnRx(rxHeader.Identifier, rxData, len);

这份 ZIP 的设计原则（你会很舒服）
1️⃣ 所有功能都有“落脚点”

你提到的 11+2 项功能，现在都有明确函数名：

参数 / 出厂相关
AdjusterParams_SetName(...)
AdjusterParams_SetCurrent(...)
AdjusterParams_SetFrequency(...)
AdjusterParams_SetAccelDecel(...)
AdjusterParams_SetFactoryPos(...)
AdjusterParams_SetFactoryDate(...)

运动 / 限位
AdjusterMotion_Start(...)
AdjusterMotion_Stop()
AdjusterMotion_WouldHitLimit(...)
AdjusterMotion_GotoFactory(...)

Identify / AutoRun
AdjusterIdentify_Start()
AdjusterIdentify_Stop()
AdjusterIdentify_IsActive()

AdjusterAutoRun_Start()
AdjusterAutoRun_Stop()
AdjusterAutoRun_IsActive()

协议 / 上报
AdjusterCAN_OnRx(...)
AdjusterCAN_TxStatus()
AdjusterCAN_TxHeartbeat()
AdjusterCAN_TxParamReport()



ZIP 内结构一览
adjuster_module_fw_full/
└─ Core
   ├─ Inc
   │  ├─ adjuster_types.h        // Axis / MotionMode enum
   │  ├─ adjuster_app.h
   │  ├─ adjuster_params.h
   │  ├─ adjuster_motion.h
   │  ├─ adjuster_identify.h
   │  ├─ adjuster_autorun.h
   │  └─ adjuster_can.h
   │
   └─ Src
      ├─ adjuster_app.c
      ├─ adjuster_params.c
      ├─ adjuster_motion.c
      ├─ adjuster_identify.c
      ├─ adjuster_autorun.c
      └─ adjuster_can.c