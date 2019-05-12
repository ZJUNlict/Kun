/************************************************************************/
/* Copyright (c) CSC-RL, Zhejiang University							*/
/* Team:			SSL-ZJUNlict										*/
/* HomePage: http://www.nlict.zju.edu.cn/ssl/WelcomePage.html			*/
/************************************************************************/
/* File:	ActionModule.h												*/
/* Brief:	C++ Implementation: Action	execution						*/
/* Func:	Provide an action command send interface					*/
/* Author:	cliffyin, 2012, 08											*/
/* Refer:	NONE														*/
/* E-mail:	cliffyin007@gmail.com										*/
/************************************************************************/

#include "ActionModule.h"
#include <KickStatus.h>
#include <DribbleStatus.h>
#include <TaskMediator.h>
#include <PlayerCommandV2.h>
#include <PlayInterface.h>
#include <CommandFactory.h>
#include <PathPlanner.h>
#include <BallStatus.h>
#include "zsplugin.hpp"
#include "CommandInterface.h"
extern Semaphore decision_to_action;

CActionModule::CActionModule(CVisionModule* pVision, const CDecisionModule* pDecision)
    : _pVision(pVision), _pDecision(pDecision) {
}

CActionModule::~CActionModule(void) {

}

// 用于当场上有的车号大于5的情况

bool CActionModule::sendAction(unsigned char robotIndex[]) {
    decision_to_action.wait();
    /************************************************************************/
    /* 第一步：遍历小车，执行赋予的任务，生成动作指令                       */
    /************************************************************************/
//    cout << "markdebug : ";
    for (int vecNum = 1; vecNum <= Param::Field::MAX_PLAYER; ++ vecNum) {
        // 获取当前小车任务
//        cout << int(robotIndex[vecNum - 1]) << ' ';
        if (robotIndex[vecNum - 1] < 0 || robotIndex[vecNum - 1] > 13) {
            cout << "Invalid number in actionmodule : " << int(robotIndex[vecNum - 1]) << endl;
        }
        CPlayerTask* pTask = TaskMediator::Instance()->getPlayerTask(vecNum);
        // 没有任务，跳过
        if (NULL == pTask) {
//            cout << "null ";
            continue;
        }

        // 执行skill，任务层层调用执行，得到最终的指令：<vx vy w> + <kick dribble>
        // 执行的结果：命令接口（仿真-DCom，实物-CommandSender） + 指令记录（运动-Vision，踢控-PlayInterface)
        bool dribble = false;
        CPlayerCommand* pCmd = NULL;
        pCmd = pTask->execute(_pVision);

        // 跑：有效的运动指令
        if (pCmd) {
            dribble = pCmd->dribble() > 0;
            // 下发运动 <vx vy w>
            pCmd->execute(robotIndex[vecNum - 1]); //this by Wang
            // 记录指令
            _pVision->SetPlayerCommand(pCmd->number(), pCmd);
        }

        // 踢：有效的踢控指令
        double kickPower = 0.0;
        double chipkickDist = 0.0;
        double passdist = 0.0;
        if (KickStatus::Instance()->needKick(vecNum)) {
            // 更新踢相关参数
            kickPower = KickStatus::Instance()->getKickPower(vecNum);
            chipkickDist = KickStatus::Instance()->getChipKickDist(vecNum);
            passdist = KickStatus::Instance()->getPassDist(vecNum);
            // 涉及到平/挑射分档，这里只关系相关参数，实际分档请关注 CommandSender
            CPlayerKickV2 kickCmd(vecNum, kickPower, chipkickDist, passdist, dribble);
            // 机构动作 <kick dribble>
            kickCmd.execute(robotIndex[vecNum - 1]);
        } else {
            CPlayerKickV2 kickCmd(vecNum, 0, 0, 0, dribble);
            kickCmd.execute(robotIndex[vecNum - 1]);
        }

        // 记录命令
        BallStatus::Instance()->setCommand(vecNum, kickPower, chipkickDist, dribble, _pVision->Cycle());
    }
//    cout << endl;
    /************************************************************************/
    /* 第二步：指令清空处理                                                 */
    /************************************************************************/
    // 清除上一周期的射门指令
    KickStatus::Instance()->clearAll();
    // 清除上一周期的控球指令
    DribbleStatus::Instance()->clearDribbleCommand();
    // 清除上一周期的障碍物标记
    CPathPlanner::resetObstacleMask();

    return true;
}

bool CActionModule::sendNoAction(unsigned char robotIndex[]) {
    for (int vecNum = 1; vecNum <= Param::Field::MAX_PLAYER; ++ vecNum) {
        // 生成停止命令
        CPlayerCommand *pCmd = CmdFactory::Instance()->newCommand(CPlayerSpeedV2(vecNum, 0, 0, 0, 0));
        // 执行且下发
        pCmd->execute(robotIndex[vecNum - 1]);
        // 记录指令
        _pVision->SetPlayerCommand(pCmd->number(), pCmd);
    }

    return true;
}

void CActionModule::stopAll() {
    for (int vecNum = 1; vecNum <= Param::Field::MAX_PLAYER; ++ vecNum) {
        // 生成停止命令
        CPlayerCommand *pCmd = CmdFactory::Instance()->newCommand(CPlayerSpeedV2(vecNum, 0, 0, 0, 0));
        // 执行且下发
        pCmd->execute(vecNum);
        // 指令记录
        _pVision->SetPlayerCommand(pCmd->number(), pCmd);
    }
    return ;
}
