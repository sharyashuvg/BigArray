#pragma once
#include<stdio.h>
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include<type_traits>
#include<exception>
#include<mutex>
#include<bit>

/*
maybe add constexpr versions but there is a problem accessing the file with it
maybe use semaphores instead of mutexes and then we'll need paging
*/




namespace BigArray {
	using namespace std;

#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#elif defined(__GNUC__) && __GNUC__ > 3
    // Clang also defines __GNUC__ (as 4)
#define FORCE_INLINE inline __attribute__ ((__always_inline__))
#else
#define FORCE_INLINE inline
#endif

#if defined(_MSC_VER)
#define fRead(_Buffer,_ElementSize,_ElementCount,_Stream) _fread_nolock(_Buffer, _ElementSize, _ElementCount, _Stream)
#define fWrite(_Buffer,_ElementSize,_ElementCount,_Stream) _fwrite_nolock(_Buffer, _ElementSize, _ElementCount, _Stream)
#define fSeek(_Stream,_Offset,_Origin) _fseeki64_nolock(_Stream,_Offset,_Origin)
#define fClose(_Stream) _fclose_nolock(_Stream)
#else
#define fRead(_Buffer,_ElementSize,_ElementCount,_Stream) fread(_Buffer, _ElementSize, _ElementCount, _Stream)
#define fWrite(_Buffer,_ElementSize,_ElementCount,_Stream) fwrite(_Buffer, _ElementSize, _ElementCount, _Stream)
#define fSeek(_Stream,_Offset,_Origin) _fseeki64(_Stream,_Offset,_Origin)
#define fClose(_Stream) fclose(_Stream)
#endif

#define uninitialized_type(type,name) \
union { \
type name; \
};
    
    template<typename T, class = void>
    struct param_type_helper {
        using type = T&;
    };
    template<typename T>
    struct param_type_helper<T, void_t<enable_if_t<(sizeof(T) <= sizeof(void*) * 2) && is_trivially_copyable_v<T>, int>>> {
        using type = T;
    };

    template<typename T>
    using param_type = param_type_helper<T>::type;


    struct OpenNew {};
    struct OpenExisting{};
    struct Both{};
    
    /*
    parameters for BigArray
    FavorSpeed: fasten the read and write operations by padding all elements to the next power of 2 thus making multiplication and division bit operations
    ConcurrentSafe:lock the read\write operations of this process
    StaticFile:make the size function return member size_t that will be updated every write if needed and once in construction
        warning: not implemented yet for now does nothing
    */
    template<typename type, bool FavorSpeed = true, bool ConcurrentSafe = false, bool StaticFile = false >
	class BigArray {

#ifndef _USE_WITH_TYPE_THAT_ISNT_TRIVIALLY_COPYABLE
        static_assert(is_trivially_copyable_v<type>,"type must be trivially_copyable\nif you want to use the class with non-trivially copyable types define _USE_WITH_TYPE_THAT_ISNT_TRIVIALLY_COPYABLE");
#endif
private:

    //so the type in reference will be changed when adding parameters to template
    using self_type = BigArray<type, FavorSpeed, ConcurrentSafe, StaticFile>;
	
    public:
		FORCE_INLINE BigArray(char const* FileName, Both _ = Both()){
            fopen_s(&_f, FileName, "r+b");
            if (!_f) fopen_s(&_f, FileName, "w+b");
			if (!_f) throw runtime_error("problem opening the file");

            if constexpr (StaticFile) {
                _size = this->bytes >> f_type_size_factor;
            }
		}
        FORCE_INLINE BigArray(char const* FileName, OpenExisting) {
            fopen_s(&_f, FileName, "r+b");
            if (!_f) throw runtime_error("problem opening the file");
        }
        FORCE_INLINE BigArray(char const* FileName, OpenNew) {
            fopen_s(&_f, FileName, "w+b");
            if (!_f) throw runtime_error("problem opening the file");
        }
        ~BigArray() {
            fClose(_f);
        }

        class reference {
            friend BigArray<type, FavorSpeed,ConcurrentSafe>;
        public:
            ~reference() noexcept {} 
            FORCE_INLINE reference& operator=(const param_type<type> _Val) noexcept {
                _Parr->_Write(_Pos, &_Val);
                return *this;
            }

            FORCE_INLINE reference& operator=(const reference _Bitref) noexcept {
                _Parr->_Write(_Pos, &static_cast<type>(_Bitref));
                return *this;
            }

            FORCE_INLINE operator type() const noexcept {
                return _Parr->_Read(_Pos);
            }
        private:

            reference(self_type* Barr, const size_t _Pos) noexcept : _Parr(Barr), _Pos(_Pos) {}

            self_type* _Parr;
            size_t _Pos; // position of element in bitset
        };
	private:
        static constexpr size_t type_size = sizeof(type);
        static constexpr size_t f_type_size = FavorSpeed ? bit_ceil(type_size) : type_size;
        static constexpr size_t f_type_size_factor = countr_zero(f_type_size);
        
        FILE* _f;
        mutable std::mutex _mutex;

        //didn't implement yet
        size_t _size;//should add this to _Compressed_pair and add template that turns is to empty if StaticFile is of

        type _Read(size_t pos) {
            uninitialized_type(type, data)
            if constexpr (ConcurrentSafe) {
                _mutex.lock();
            }
            fSeek(_f, pos, SEEK_SET);
            fRead(&data, type_size, 1, _f);
            if constexpr (ConcurrentSafe) {
                _mutex.unlock();
            }
            return data;
        }
        void _Write(size_t pos,const type* data) {
            if constexpr (ConcurrentSafe) {
                _mutex.lock();
            }
            fSeek(_f, pos, SEEK_SET);
            fWrite(data, type_size, 1, _f);
            if constexpr (ConcurrentSafe) {
                _mutex.unlock();
            }
        }

    public:

        FORCE_INLINE reference operator[](size_t pos) {
            return reference(this, pos << f_type_size_factor );
        }

        FORCE_INLINE type operator[](size_t pos) const {
            return this->_Read(pos << f_type_size_factor);
        }

        //slow but safe
        type at(size_t pos) const {
            if(pos>this->size()) throw std::out_of_range("tried access element out of range with at function");
            return this->_Read(pos << f_type_size_factor);
        }


        size_t Real_bytes() const {
            if constexpr (ConcurrentSafe) {
                _mutex.lock();
            }
            fSeek(_f, 0, SEEK_END);
            int size = ftell(_f);
            if constexpr (ConcurrentSafe) {
                _mutex.unlock();
            }
            return size;
        }

        size_t bytes() const {
            return this->Real_bytes()-type_size+f_type_size;
        }

        FORCE_INLINE size_t size() const {
            return ((this->Real_bytes()-1) >> f_type_size_factor)+1;
        }


	};

}
