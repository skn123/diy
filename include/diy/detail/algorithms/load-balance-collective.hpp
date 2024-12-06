#pragma once

#include <queue>
#include "load-balance.hpp"

namespace diy
{

namespace detail
{

// exchange work information among all processes using synchronous collective method
template<class Block>
void exchange_work_info(diy::Master&            master,
                        const WorkInfo&         my_work_info,           // my process' work info
                        std::vector<WorkInfo>&  all_work_info)          // (output) global work info
{
    auto nprocs = master.communicator().size();     // global number of procs
    all_work_info.resize(nprocs);
    diy::mpi::detail::all_gather(master.communicator(), &my_work_info.proc_rank,
            sizeof(WorkInfo) / sizeof(WorkInfo::proc_rank), MPI_INT, &all_work_info[0].proc_rank);  // assumes all elements of WorkInfo are sizeof(int)
}

// determine move info from work info
void decide_move_info(std::vector<WorkInfo>&        all_work_info,          // global work info
                      std::vector<MoveInfo>&        all_move_info)          // (output) move info for all moves
{
    all_move_info.clear();

    // move all blocks with an approximation to the longest processing time first (LPTF) scheduling algorithm
    // https://en.wikipedia.org/wiki/Longest-processing-time-first_scheduling
    // we iteratively move the heaviest block to lightest proc
    // constrained by only recording the heaviest block for each proc, not all blocks

    // minimum proc_work priority queue, top is min proc_work
    auto cmp = [&](WorkInfo& a, WorkInfo& b) { return a.proc_work > b.proc_work; };
    std::priority_queue<WorkInfo, std::vector<WorkInfo>, decltype(cmp)> min_proc_work_q(cmp);
    for (auto i = 0; i < all_work_info.size(); i++)
        min_proc_work_q.push(all_work_info[i]);

    // sort all_work_info by descending top_work
    std::sort(all_work_info.begin(), all_work_info.end(),
            [&](WorkInfo& a, WorkInfo& b) { return a.top_work > b.top_work; });

    // walk the all_work_info vector in descending top_work order
    // move the next heaviest block to the lightest proc
    for (auto i = 0; i < all_work_info.size(); i++)
    {
        auto src_work_info = all_work_info[i];                      // heaviest block
        auto dst_work_info = min_proc_work_q.top();                 // lightest proc

        // sanity check that the move makes sense
        if (src_work_info.proc_work - dst_work_info.proc_work > src_work_info.top_work &&   // improve load balance
                src_work_info.proc_rank != dst_work_info.proc_rank &&                       // not self
                src_work_info.nlids > 1)                                                    // don't leave a proc with no blocks
        {
            MoveInfo move_info;
            move_info.src_proc  = src_work_info.proc_rank;
            move_info.dst_proc  = dst_work_info.proc_rank;
            move_info.move_gid  = src_work_info.top_gid;
            all_move_info.push_back(move_info);

            // update the min_proc_work priority queue
            auto work_info = min_proc_work_q.top();                 // lightest proc
            work_info.proc_work += src_work_info.top_work;
            if (work_info.top_work < src_work_info.top_work)
            {
                work_info.top_work = src_work_info.top_work;
                work_info.top_gid  = src_work_info.top_gid;
            }
            min_proc_work_q.pop();
            min_proc_work_q.push(work_info);
        }
    }
}

// move one block from src to dst proc
template<class Block>
void move_block(diy::DynamicAssigner&   assigner,
                diy::Master&            master,
                const MoveInfo&         move_info)
{
    // update the dynamic assigner
    if (master.communicator().rank() == move_info.src_proc)
        assigner.set_rank(move_info.dst_proc, move_info.move_gid, true);

    // move the block from src to dst proc
    void* send_b;
    Block* recv_b;
    if (master.communicator().rank() == move_info.src_proc)
    {
        send_b = master.block(master.lid(move_info.move_gid));
        diy::MemoryBuffer bb;
        master.saver()(send_b, bb);
        master.communicator().send(move_info.dst_proc, 0, bb.buffer);
    }
    else if (master.communicator().rank() == move_info.dst_proc)
    {
        recv_b = static_cast<Block*>(master.creator()());
        diy::MemoryBuffer bb;
        master.communicator().recv(move_info.src_proc, 0, bb.buffer);
        recv_b->load(recv_b, bb);
    }

    // move the link for the moving block from src to dst proc and update master on src and dst proc
    if (master.communicator().rank() == move_info.src_proc)
    {
        diy::Link* send_link = master.link(master.lid(move_info.move_gid));
        diy::MemoryBuffer bb;
        diy::LinkFactory::save(bb, send_link);
        master.communicator().send(move_info.dst_proc, 0, bb.buffer);

        // remove the block from the master
        int move_lid = master.lid(move_info.move_gid);
        delete master.release(move_lid);
    }
    else if (master.communicator().rank() == move_info.dst_proc)
    {
        diy::MemoryBuffer bb;
        diy::Link* recv_link;
        master.communicator().recv(move_info.src_proc, 0, bb.buffer);
        recv_link = diy::LinkFactory::load(bb);

        // add block to the master
        master.add(move_info.move_gid, recv_b, recv_link);
    }
}

}   // detail

}   // diy

