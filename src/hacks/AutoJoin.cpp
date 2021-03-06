/*
 * AutoJoin.cpp
 *
 *  Created on: Jul 28, 2017
 *      Author: nullifiedcat
 */

#include <settings/Int.hpp>
#include "HookTools.hpp"
#include <hacks/AutoJoin.hpp>

#include "common.hpp"
#include "hack.hpp"

static settings::Bool autojoin_team{ "autojoin.team", "false" };
static settings::Int autojoin_class{ "autojoin.class", "0" };
static settings::Bool auto_queue{ "autojoin.auto-queue", "false" };
static settings::Bool party_bypass{ "autojoin.party-bypass", "false" };
static settings::Bool auto_requeue{ "autojoin.auto-requeue", "false" };

namespace hacks::shared::autojoin
{

/*
 * Credits to Blackfire for helping me with auto-requeue!
 */

const std::string classnames[] = { "scout",   "sniper", "soldier",
                                   "demoman", "medic",  "heavyweapons",
                                   "pyro",    "spy",    "engineer" };

bool UnassignedTeam()
{
    return !g_pLocalPlayer->team or (g_pLocalPlayer->team == TEAM_SPEC);
}

bool UnassignedClass()
{
    return g_pLocalPlayer->clazz != *autojoin_class;
}

static Timer autoteam_timer{};
static Timer startqueue_timer{};
#if not ENABLE_VISUALS
static Timer queue_time{};
#endif
void updateSearch()
{
    // segfaults for no reason
    static bool calld = false;
    /*
    if (party_bypass && !calld)
    {
        static unsigned char patch[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
        uintptr_t party_addr1        = gSignatures.GetClientSignature(
            "55 89 E5 57 56 53 81 EC ? ? ? ? 8B 45 ? 8B 5D ? 8B 55 ? 89 85 ? ? "
            "? ? 65 A1 ? ? ? ? 89 45 ? 31 C0 8B 45");
        static unsigned char patch2[] = { 0x90, 0xE9 };
        uintptr_t party_addr2         = gSignatures.GetClientSignature(
            "55 89 E5 57 56 53 81 EC ? ? ? ? C7 85 ? ? ? ? ? ? ? ? C7 85 ? ? ? "
            "? ? ? ? ? 8B 45 ? 89 85 ? ? ? ? 65 A1 ? ? ? ? 89 45 ? 31 C0 A1 ? "
            "? ? ? 85 C0 0F 84");
        static unsigned char patch3[] = { 0x90, 0x90 };
        uintptr_t party_addr3         = gSignatures.GetClientSignature(
            "8D 65 ? 5B 5E 5F 5D C3 31 F6 8B 87");
        static unsigned char patch4[] = { 0x90, 0xE9 };
        static unsigned char patch5[] = { 0x90, 0x90, 0x90, 0x90 };
        uintptr_t party_addr4         = gSignatures.GetClientSignature(
            "55 89 E5 57 56 53 83 EC ? 8B 5D ? 8B 75 ? 8B 7D ? 89 1C 24 89 74 24
    ? 89 7C 24 ? E8 ? ? ? ? 84 C0 74 ? 83 C4");
        if (!party_addr1 || !party_addr2 || !party_addr3 || !party_addr4)
            logging::Info("Invalid Party signature");
        else
        {
            Patch((void *) (party_addr1 + 0x41), (void *) patch, sizeof(patch));
            Patch((void *) (party_addr2 + 0x9FA), (void *) patch2,
                  sizeof(patch2));
            Patch((void *) (party_addr3 + 0xC5), (void *) patch3,
                  sizeof(patch3));
            Patch((void *) (party_addr3 + 0xF0), (void *) patch4,
                  sizeof(patch4));
            Patch((void *) (party_addr4 + 0x7F), (void *) patch5,
                  sizeof(patch5));
            calld = true;
        }
    }*/

    if (!auto_queue && !auto_requeue)
    {
#if not ENABLE_VISUALS
        queue_time.update();
#endif
        return;
    }
    if (g_IEngine->IsInGame())
    {
#if not ENABLE_VISUALS
        queue_time.update();
#endif
        return;
    }

    static uintptr_t addr =
        gSignatures.GetClientSignature("C7 04 24 ? ? ? ? 8D 7D ? 31 F6");
    static uintptr_t offset0 = uintptr_t(*(uintptr_t *) (addr + 0x3));
    static uintptr_t offset1 = gSignatures.GetClientSignature(
        "55 89 E5 83 EC ? 8B 45 ? 8B 80 ? ? ? ? 85 C0 74 ? C7 44 24 ? ? ? ? ? "
        "89 04 24 E8 ? ? ? ? 85 C0 74 ? 8B 40");
    typedef int (*GetPendingInvites_t)(uintptr_t);
    GetPendingInvites_t GetPendingInvites = GetPendingInvites_t(offset1);
    int invites                           = GetPendingInvites(offset0);

    re::CTFGCClientSystem *gc = re::CTFGCClientSystem::GTFGCClientSystem();
    re::CTFPartyClient *pc    = re::CTFPartyClient::GTFPartyClient();

    if (current_user_cmd && gc && gc->BConnectedToMatchServer(false) &&
        gc->BHaveLiveMatch())
    {
#if not ENABLE_VISUALS
        queue_time.update();
#endif
        tfmm::leaveQueue();
    }
    //    if (gc && !gc->BConnectedToMatchServer(false) &&
    //            queuetime.test_and_set(10 * 1000 * 60) &&
    //            !gc->BHaveLiveMatch())
    //        tfmm::leaveQueue();

    if (auto_requeue)
    {
        if (startqueue_timer.check(5000) && gc &&
            !gc->BConnectedToMatchServer(false) && !gc->BHaveLiveMatch() &&
            !invites)
            if (pc && !(pc->BInQueueForMatchGroup(tfmm::getQueue()) ||
                        pc->BInQueueForStandby()))
            {
                logging::Info("Starting queue for standby, Invites %d",
                              invites);
                tfmm::startQueueStandby();
            }
    }

    if (auto_queue)
    {
        if (startqueue_timer.check(5000) && gc &&
            !gc->BConnectedToMatchServer(false) && !gc->BHaveLiveMatch() &&
            !invites)
            if (pc && !(pc->BInQueueForMatchGroup(tfmm::getQueue()) ||
                        pc->BInQueueForStandby()))
            {
                logging::Info("Starting queue, Invites %d", invites);
                tfmm::startQueue();
            }
    }
    startqueue_timer.test_and_set(5000);
#if not ENABLE_VISUALS
    if (queue_time.test_and_set(1200000))
    {
        *(int *) nullptr = 0;
    }
#endif
}
static HookedFunction
    update(HookedFunctions_types::HF_CreateMove, "Autojoin", 1, []() {
#if !LAGBOT_MODE
        if (autoteam_timer.test_and_set(500))
        {
            if (autojoin_team and UnassignedTeam())
            {
                hack::ExecuteCommand("autoteam");
            }
            else if (autojoin_class and UnassignedClass())
            {
                if (int(autojoin_class) < 10)
                    g_IEngine->ExecuteClientCmd(
                        format("join_class ",
                               classnames[int(autojoin_class) - 1])
                            .c_str());
            }
        }
#endif
    });

void onShutdown()
{
    if (auto_queue)
        tfmm::startQueue();
}
} // namespace hacks::shared::autojoin
