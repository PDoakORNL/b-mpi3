#if COMPILATION_INSTRUCTIONS
(echo "#include<"$0">" > $0x.cpp) && mpicxx -O3 -std=c++14 -Wall -Wfatal-errors -D_TEST_BOOST_MPI3_SHARED_WINDOW $0x.cpp -o $0x.x && time mpirun -np 3 $0x.x $@ && rm -f $0x.x $0x.cpp; exit
#endif
#ifndef BOOST_MPI3_SHARED_WINDOW_HPP
#define BOOST_MPI3_SHARED_WINDOW_HPP

#include "../mpi3/shared_communicator.hpp"
#include "../mpi3/dynamic_window.hpp"

#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

namespace boost{
namespace mpi3{

template<class T /*= void*/>
struct shared_window : window<T>{
	shared_window(shared_communicator& comm, mpi3::size_t n, int disp_unit) 
	: window<T>(){
		void* base_ptr = nullptr;
		int s = MPI_Win_allocate_shared(n, disp_unit, MPI_INFO_NULL, comm.impl_, &base_ptr, &this->impl_);
		if(s != MPI_SUCCESS) throw std::runtime_error("cannot create shared window");
	}
	shared_window(shared_communicator& comm, int disp_unit) : shared_window(comm, 0, disp_unit){}
	using query_t = std::tuple<mpi3::size_t, int, void*>;
	query_t query(int rank = MPI_PROC_NULL) const{
		query_t ret;
		MPI_Win_shared_query(this->impl_, rank, &std::get<0>(ret), &std::get<1>(ret), &std::get<2>(ret));
		return ret;
	}
	template<class TT = char>
	mpi3::size_t size(int rank = MPI_PROC_NULL) const{return std::get<0>(query(rank))/sizeof(T);}
	int disp_unit(int rank = MPI_PROC_NULL) const{return std::get<1>(query(rank));}
	template<class TT = void>
	T* base(int rank = MPI_PROC_NULL) const{return static_cast<T*>(std::get<2>(query(rank)));}
//	template<class T = char>
//	void attach_n(T* base, mpi3::size_t n){MPI_Win_attach(impl_, base, n*sizeof(T));}
};

#if 0
struct managed_shared_memory{
	shared_window<> sw_;
	managed_shared_memory(shared_communicator& c, int s) : sw_(c, c.rank()==0?s:0){}
//	struct segment_manager{};
	using segment_manager = boost::interprocess::segment_manager<char, boost::interprocess::rbtree_best_fit<boost::interprocess::mutex_family>, boost::interprocess::iset_index>;
	segment_manager sm_;
	managed_shared_memory::segment_manager* get_segment_manager(){
		return &sm_;
	}
};
#endif

template<class T /*= char*/> 
shared_window<T> shared_communicator::make_shared_window(
	mpi3::size_t size, 
	int disp_unit /*= sizeof(T)*/
){
	return shared_window<T>(*this, size*sizeof(T), disp_unit);
}

template<class T /*= char*/>
shared_window<T> shared_communicator::make_shared_window(){
	return shared_window<T>(*this, sizeof(T));
}

namespace intranode{

template<class T> struct array_ptr;

template<>
struct array_ptr<const void>{
	using T = const void;
	std::shared_ptr<shared_window<>> wSP_;
	std::ptrdiff_t offset = 0;
	array_ptr(std::nullptr_t = nullptr){}
	array_ptr(array_ptr const& other) = default;
	array_ptr& operator=(array_ptr const& other) = default;
};

template<>
struct array_ptr<void>{
	using T = void;
	std::shared_ptr<shared_window<>> wSP_;
	std::ptrdiff_t offset = 0;
	array_ptr(std::nullptr_t = nullptr){}
	array_ptr(array_ptr const& other) = default;
	array_ptr& operator=(array_ptr const& other) = default;
};

template<class T>
struct array_ptr{
	std::shared_ptr<shared_window<T>> wSP_;
	std::ptrdiff_t offset = 0;
	array_ptr(){}
	array_ptr(std::nullptr_t){}
//	array_ptr(std::nullptr_t = nullptr) : offset(0){}
	array_ptr(array_ptr const& other) = default;
//	array_ptr(T* const& other = nullptr) : offset(0){}
//	array_ptr(T* const& other = nullptr) : offset(0){}
	array_ptr& operator=(array_ptr const& other) = default;
	T& operator*() const{return *((T*)(wSP_->base(0)) + offset);}
	T& operator[](int idx) const{return ((T*)(wSP_->base(0)) + offset)[idx];}
	T* operator->() const{return (T*)(wSP_->base(0)) + offset;}
	explicit operator bool() const{return (bool)wSP_;}//.get();}
	operator array_ptr<void const>() const{
		array_ptr<void const> ret;
		ret.wSP_ = wSP_;
		return ret;
	}
	array_ptr operator+(std::ptrdiff_t d) const{
		array_ptr ret(*this);
		ret += d;
		return ret;
	}
	std::ptrdiff_t operator-(array_ptr other) const{
		return offset - other.offset;
	}
	array_ptr& operator--(){--offset; return *this;}
	array_ptr& operator++(){++offset; return *this;}
	array_ptr& operator-=(std::ptrdiff_t d){offset -= d; return *this;}
	array_ptr& operator+=(std::ptrdiff_t d){offset += d; return *this;}
	bool operator==(array_ptr<T> const& other) const{
		return wSP_->base(0) == other.wSP_->base(0) and offset == other.offset;
	}
	bool operator!=(array_ptr<T> const& other) const{return not((*this)==other);}
	bool operator<=(array_ptr<T> const& other) const{
		return wSP_->base(0) + offset <= other.wSP_->base(0) + other.offset;
	}
};

template<class T> struct allocator{
	using value_type = T;
	using pointer = array_ptr<T>;
	using const_pointer = array_ptr<T const>;
	using reference = T&;
	using const_reference = T const&;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	mpi3::shared_communicator& comm_;
	allocator(mpi3::shared_communicator& comm) : comm_(comm){}
	template<class U>
	allocator(allocator<U> const& other) : comm_(other.comm_){}

	array_ptr<T> allocate(size_type n, const void* hint = 0){
		array_ptr<T> ret;
		if(n == 0) return ret;
		ret.wSP_ = std::make_shared<shared_window>(
			comm_.make_shared_window<T>(comm_.rank()==0?n:0)
		//	comm_.allocate_shared(comm_.rank()==0?n*sizeof(T):1)
		);
		return ret;
	}
	void deallocate(array_ptr<T> ptr, size_type){
		ptr.wSP_.reset();
	}
//	void deallocate(double* const&, std::size_t&){}
	bool operator==(allocator const& other) const{
		return comm_ == other.comm_;
	}
	bool operator!=(allocator const& other) const{
		return not (other == *this);
	}
};

}


}}

#ifdef _TEST_BOOST_MPI3_SHARED_WINDOW

#include "alf/boost/mpi3/main.hpp"

namespace mpi3 = boost::mpi3; using std::cout;

int mpi3::main(int argc, char* argv[], mpi3::communicator& world){

	mpi3::shared_communicator node = world.split_shared();
	mpi3::shared_window<int> win = node.make_shared_window<int>(node.rank()==0?1:0);

	assert(win.base() != nullptr and win.size<int>() == 1);

	win.lock_all();
	if(node.rank()==0) *win.base<int>(0) = 42;
	for (int j=1; j != node.size(); ++j){
		if(node.rank()==0) node.send_n((int*)nullptr, 0, j);//, 666);
	    else if(node.rank()==j) node.receive_n((int*)nullptr, 0, 0);//, 666);
	}
	win.sync();

	int l = *win.base<int>(0);
	win.unlock_all();

	int minmax[2] = {-l,l};
	node.all_reduce_n(&minmax[0], 2, mpi3::max<>{});
	assert( -minmax[0] == minmax[1] );
	cout << "proc " << node.rank() << " " << l << std::endl;

	return 0;
}

#endif
#endif

