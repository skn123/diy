#include <vector>

#include "operations.hpp"

// TODO: add scan and gather

namespace diy
{
namespace mpi
{
  /* Broadcast */
  template<class T, class Op>
  struct Collectives
  {
    typedef   detail::mpi_datatype<T>     Datatype;

    static void broadcast(const communicator& comm, T& x, int root)
    {
      MPI_Bcast(Datatype::address(x),
                Datatype::count(x),
                Datatype::datatype(), root, comm);
    }

    static void reduce(const communicator& comm, const T& in, T& out, int root, const Op&)
    {
      MPI_Reduce(Datatype::address(in),
                 Datatype::address(out),
                 Datatype::count(in),
                 Datatype::datatype(),
                 detail::mpi_op<Op>::get(),
                 root, comm);
    }

    static void reduce(const communicator& comm, const T& in, int root, const Op& op)
    {
      MPI_Reduce(Datatype::address(in),
                 Datatype::address(const_cast<T&>(in)),
                 Datatype::count(in),
                 Datatype::datatype(),
                 detail::mpi_op<Op>::get(),
                 root, comm);
    }

    static void all_to_all(const communicator& comm, const std::vector<T>& in, std::vector<T>& out, int n = 1)
    {
      // NB: this will fail if T is a vector
      MPI_Alltoall(Datatype::address(&in[0]), n,
                   Datatype::datatype(),
                   Datatype::address(&out[0]), n,
                   Datatype::datatype(),
                   comm);
    }
  };

  template<class T>
  void      broadcast(const communicator& comm, T& x, int root)
  {
    Collectives<T,void*>::broadcast(comm, x, root);
  }

  template<class T, class Op>
  void      reduce(const communicator& comm, const T& in, T& out, int root, const Op& op)
  {
    Collectives<T, Op>::reduce(comm, in, out, root, op);
  }

  // Should not be called on the root process
  template<class T, class Op>
  void      reduce(const communicator& comm, const T& in, int root, const Op& op)
  {
    Collectives<T, Op>::reduce(comm, in, root, op);
  }

  template<class T>
  void      all_to_all(const communicator& comm, const std::vector<T>& in, std::vector<T>& out, int n = 1)
  {
    Collectives<T, void*>::all_to_all(comm, in, out, n);
  }
}
}
