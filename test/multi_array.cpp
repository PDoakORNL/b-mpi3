#if COMPILATION_INSTRUCTIONS
mpic++ -O3 -std=c++14 -Wall -Wextra `#-Wfatal-errors` -I$HOME/prj/alf $0 -o $0x.x && mpirun -n 4 $0x.x $@ && rm -f $0x.x; exit
#endif

#include "../../mpi3/shm/allocator.hpp"
#include "../../mpi3/main.hpp"

#include "boost/multi/array.hpp"

namespace mpi3 = boost::mpi3;
namespace multi = boost::multi;

namespace boost::mpi3::shm::multi{
	template<class T, int D>
	using array = boost::multi::array<T, D, mpi3::shm::allocator<T>>;
}

int mpi3::main(int, char*[], mpi3::communicator world){

	mpi3::shared_communicator node = world.split_shared();

	multi::array<double, 2, shm::allocator<double>> A({16, 16}, node);	
	assert(A[2][2] == 0);
	node.barrier();
	if(node.root()) A[2][2] = 3.14;
	node.barrier();
	assert(A[2][2] == 3.14);

	shm::multi::array<double, 2> B = A;
//	multi::array<double, 2, mpi3::shm::allocator<double>> B = A;
	assert(A[2][2] == 3.14);
	assert(B[2][2] == 3.14);
	
	shm::multi::array<double, 2> C({1, 1}, node);
	C.reextent({30,30});
	assert(C.size() == 30);
//	C = std::move(A);
//	assert(C[2][2] == 3.14);
//	assert(A.size()==0);
	node.barrier();
		
	{
		std::vector<shm::multi::array<double, 2>> w;
		std::vector<shm::multi::array<double, 2>> v(2, {node});
		v.reserve(3);
		v.emplace_back(shm::multi::array<double, 2>({10,10}, node));
		v.emplace_back(node);
		v.emplace_back(shm::multi::array<double, 2>({20,20}, node));
		v.emplace_back(shm::multi::array<double, 2>({15,15}, node));


		if(world.rank() == 0) v[4][2][3] = 5.;
		world.barrier();
		if(world.rank() == 1) assert(v[4][2][3] == 5.);
		world.barrier();
		v[2].reextent({30, 30});
		assert( v[2].size() == 30 );
		v.pop_back();
		assert( v.back().size() == 20 );
	}
	shm::multi::array<double, 2> AA({0,0}, node);
	assert(AA.size() == 0);
	{
		boost::multi::array<shm::multi::array<double, 2>, 1> v({{3}}, {node});
		v[0].reextent({10, 10});
		v[1].reextent({20, 20});
		v[2].reextent({30, 30});
		if(world.rank() == 1) v[1][2][3] = 5.;
		world.barrier();
		if(world.rank() == 1) assert(v[1][2][3] == 5.);
	}
	return 0;
}

