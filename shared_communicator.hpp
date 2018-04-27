#if COMPILATION_INSTRUCTIONS
(echo "#include\""$0"\"" > $0x.cpp) && mpic++ -O3 -std=c++14 -Wall -Wextra -Wfatal-errors -D_TEST_MPI3_SHARED_COMMUNICATOR $0x.cpp -o $0x.x && time mpirun -n 3 $0x.x $@ && rm -f $0x.x $0x.cpp; exit
#endif
#ifndef MPI3_SHARED_COMMUNICATOR_HPP
#define MPI3_SHARED_COMMUNICATOR_HPP

#include "../mpi3/communicator.hpp"
#include "../mpi3/environment.hpp" // processor_name

namespace boost{
namespace mpi3{

template<class T = void>
struct shared_window;

struct shared_communicator : communicator{
private:
	template<class T> static auto data_(T&& t){
		using detail::data;
		return data(std::forward<T>(t));
	}
	shared_communicator(communicator&& c) : communicator(std::move(c)){}
	inline shared_communicator(communicator const& comm, int key = 0){
		int s = MPI_Comm_split_type(&comm, MPI_COMM_TYPE_SHARED, key, MPI_INFO_NULL, &impl_);
		if(s != MPI_SUCCESS) throw std::runtime_error("cannot split shared");
		name(mpi3::processor_name());
	}
	friend class communicator;
public:
	inline shared_communicator split(int key) const{return split_shared(key);}
	shared_communicator split(int color, int key) const{
		return communicator::split(color, key);
	}

	template<class T = char>
	shared_window<T> make_shared_window(mpi3::size_t size);
	template<class T = char>
	shared_window<T> make_shared_window();
};

inline shared_communicator communicator::split_shared(int key /*= 0*/) const{
	return shared_communicator(*this, key);
}

}}

#ifdef _TEST_MPI3_SHARED_COMMUNICATOR

#include "../mpi3/shared_main.hpp"
#include "../mpi3/operation.hpp"
#include "../mpi3/shared_window.hpp"

#include<iostream>

namespace mpi3 = boost::mpi3;
using std::cout;

int mpi3::main(int argc, char* argv[], mpi3::shared_communicator node){

	auto win = node.make_shared_window<int>(node.rank()?0:1);
	assert(win.base() != nullptr and win.size() == 1);
	win.lock_all();
	if(node.rank()==0){
		*win.base() = 42;
		win.sync();
	}
	for(int j=1; j != node.size(); ++j){
		if(node.rank()==0) node.send_n((int*)nullptr, 0, j, 666);
		else if(node.rank()==j) node.receive_n((int*)nullptr, 0, 0, 666);
	}
	if(node.rank() != 0) win.sync();
	int l = *win.base();
	win.unlock_all();
	int minmax[2] = {-l,l};
//	node.reduce_in_place_n(&minmax[0], 2, mpi3::max<>{}, 0);
	node.all_reduce_n(&minmax[0], 2, mpi3::max<>{});
	assert( -minmax[0] == minmax[1] );
}

#endif
#endif

