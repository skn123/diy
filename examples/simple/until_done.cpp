#include <vector>
#include <iostream>

#include <diy/mpi.hpp>
#include <diy/communicator.hpp>
#include <diy/master.hpp>
#include <diy/assigner.hpp>
#include <diy/serialization.hpp>

struct Block
{
  int   count;
  bool  all_done;

        Block(): count(0), all_done(false)      {}
};

void*   create_block()                      { return new Block; }
void    destroy_block(void* b)              { delete static_cast<Block*>(b); }
void    save_block(const void* b,
                   diy::BinaryBuffer& bb)   { diy::save(bb, *static_cast<const Block*>(b)); }
void    load_block(void* b,
                   diy::BinaryBuffer& bb)   { diy::load(bb, *static_cast<Block*>(b)); }

void flip_coin(void* b_, const diy::Master::ProxyWithLink& cp)
{
  Block*        b = static_cast<Block*>(b_);

  b->count++;
  bool done = rand() % 2;
  //std::cout << cp.gid() << "  " << done << " " << b->count << std::endl;
  cp.all_reduce(done, offsetof(Block, all_done), std::logical_and<bool>());
}

int main(int argc, char* argv[])
{
  diy::mpi::environment     env(argc, argv);
  diy::mpi::communicator    world;

  int                       nblocks = 4*world.size();

  diy::FileStorage          storage("./DIY.XXXXXX");

  diy::Communicator         comm(world);
  diy::Master               master(comm,
                                   &create_block,
                                   &destroy_block,
                                   2,
                                   &storage,
                                   &save_block,
                                   &load_block);

  srand(time(NULL));

  //diy::ContiguousAssigner   assigner(world.size(), nblocks);
  diy::RoundRobinAssigner   assigner(world.size(), nblocks);

  for (unsigned gid = 0; gid < nblocks; ++gid)
    if (assigner.rank(gid) == world.rank())
      master.add(gid, new Block, new diy::Link);

  bool all_done = false;
  while (!all_done)
  {
    master.foreach(&flip_coin);
    comm.exchange();
    comm.flush();

    master.extract_collectives(master.loaded_block());
    all_done = master.block<Block>(master.loaded_block())->all_done;
  }

  if (world.rank() == 0)
    std::cout << "Total iterations: " << master.block<Block>(master.loaded_block())->count << std::endl;
}

