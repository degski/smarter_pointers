
// MIT License
//
// Copyright (c) 2020 degski
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <array>
#include <memory>
#include <new>
#include <sax/iostream.hpp>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

#include <experimental/fixed_capacity_vector>

#include <boost/interprocess/offset_ptr.hpp>

namespace sax {

template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
void print_bits ( T const n_ ) noexcept {
    using Tu = typename std::make_unsigned<T>::type;
    Tu n;
    std::memcpy ( &n, &n_, sizeof ( Tu ) );
    Tu i = Tu ( 1 ) << ( sizeof ( Tu ) * 8 - 1 );
    while ( i ) {
        putchar ( int ( ( n & i ) > 0 ) + int ( 48 ) );
        i >>= 1;
    }
}
}; // namespace sax

template<typename T>
class unique_ptr {

    // https://lokiastari.com/blog/2014/12/30/c-plus-plus-by-example-smart-pointer/
    // https://codereview.stackexchange.com/questions/163854/my-implementation-for-stdunique-ptr

    public:
    using value_type    = T;
    using pointer       = value_type *;
    using const_pointer = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;

    explicit unique_ptr ( ) : m_data ( nullptr ) {}
    // Explicit constructor
    explicit unique_ptr ( pointer raw ) : m_data ( raw ) {}
    ~unique_ptr ( ) {
        if ( is_unique ( ) )
            delete m_data;
    }

    // Constructor/Assignment that binds to nullptr
    // This makes usage with nullptr cleaner
    unique_ptr ( std::nullptr_t ) : m_data ( nullptr ) {}
    unique_ptr & operator= ( std::nullptr_t ) {
        reset ( );
        return *this;
    }

    // Constructor/Assignment that allows move semantics
    unique_ptr ( unique_ptr && moving ) noexcept { moving.swap ( *this ); }
    unique_ptr & operator= ( unique_ptr && moving ) noexcept {
        moving.swap ( *this );
        return *this;
    }

    // Constructor/Assignment for use with types derived from T
    template<typename U>
    explicit unique_ptr ( unique_ptr<U> && moving ) noexcept {
        unique_ptr<T> tmp ( moving.release ( ) );
        tmp.swap ( *this );
    }
    template<typename U>
    unique_ptr & operator= ( unique_ptr<U> && moving ) noexcept {
        unique_ptr<T> tmp ( moving.release ( ) );
        tmp.swap ( *this );
        return *this;
    }

    // Remove compiler generated copy semantics.
    unique_ptr ( unique_ptr const & ) = delete;
    unique_ptr & operator= ( unique_ptr const & ) = delete;

    // Const correct access owned object
    pointer operator-> ( ) const noexcept { return pointer_view ( m_data ); }
    reference operator* ( ) const { return *pointer_view ( m_data ); }

    // Access to smart pointer state
    pointer get ( ) const noexcept { return pointer_view ( m_data ); }
    pointer get ( ) noexcept { return pointer_view ( m_data ); }
    explicit operator bool ( ) const { return pointer_view ( m_data ); }

    // Modify object state
    pointer release ( ) noexcept {
        pointer result = nullptr;
        std::swap ( result, m_data );
        return pointer_view ( result );
    }
    void swap ( unique_ptr & src ) noexcept { std::swap ( m_data, src.m_data ); }

    void reset ( ) noexcept {
        pointer tmp = release ( );
        delete pointer_view ( tmp );
    }
    void reset ( pointer ptr_ = pointer ( ) ) noexcept {
        pointer result = ptr_;
        std::swap ( result, m_data );
        delete pointer_view ( result );
    }
    template<typename U>
    void reset ( unique_ptr<U> && moving_ ) noexcept {
        unique_ptr<T> result ( moving_ );
        std::swap ( result, *this );
        delete pointer_view ( result );
    }

    void weakify ( ) noexcept {
        m_data = reinterpret_cast<pointer> ( reinterpret_cast<std::uintptr_t> ( pointer_view ( m_data ) ) | weak_mask );
    }
    void uniquify ( ) noexcept { m_data &= ptr_mask; }

    void swap_ownership ( unique_ptr & other_ ) noexcept {
        auto flip = [] ( unique_ptr & u ) {
            u.m_data = reinterpret_cast<pointer> ( reinterpret_cast<std::uintptr_t> ( u.m_data ) | weak_mask );
        };
        flip ( *this );
        flip ( other_ );
    }

    [[nodiscard]] bool is_weak ( ) const noexcept {
        return static_cast<bool> ( reinterpret_cast<std::uintptr_t> ( m_data ) & weak_mask );
    }
    [[nodiscard]] bool is_unique ( ) const noexcept { return not is_weak ( ); }

    [[nodiscard]] static constexpr pointer pointer_view ( pointer p_ ) noexcept {
        return reinterpret_cast<pointer> ( reinterpret_cast<std::uintptr_t> ( p_ ) & ptr_mask );
    }

    private:
    pointer m_data;

    static constexpr std::uintptr_t ptr_mask  = 0x00FF'FFFF'FFFF'FFF0;
    static constexpr std::uintptr_t weak_mask = 0x0000'0000'0000'0001;
};

////////////////////////////////////////////////////////////////////////////////

namespace std {
template<typename T>
void swap ( unique_ptr<T> & lhs, unique_ptr<T> & rhs ) {
    lhs.swap ( rhs );
}
} // namespace std

////////////////////////////////////////////////////////////////////////////////
//
// Stephan T Lavavej (STL!) implementation of make_unique, which has been
// accepted into the C++14 standard. It includes handling for arrays. Paper
// here: http://isocpp.org/files/papers/N3656.txt
//
////////////////////////////////////////////////////////////////////////////////

namespace detail {
template<class T>
struct _Unique_if {
    typedef unique_ptr<T> _Single_object;
};
// Specialization for unbound array.
template<class T>
struct _Unique_if<T[]> {
    typedef unique_ptr<T[]> _Unknown_bound;
};
// Specialization for array of known size.
template<class T, size_t N>
struct _Unique_if<T[ N ]> {
    typedef void _Known_bound;
};
} // namespace detail

////////////////////////////////////////////////////////////////////////////////

// Specialization for normal object type.
template<class T, class... Args>
typename detail::_Unique_if<T>::_Single_object make_unique ( Args &&... args ) {
    return unique_ptr<T> ( new T ( std::forward<Args> ( args )... ) );
}
// Specialization for unknown bound.
template<class T>
typename detail::_Unique_if<T>::_Unknown_bound make_unique ( size_t size ) {
    typedef typename std::remove_extent<T>::type U;
    return unique_ptr<T> ( new U[ size ]( ) );
}
// Deleted specialization.
template<class T, class... Args>
typename detail::_Unique_if<T>::_Known_bound make_unique ( Args &&... ) = delete;

////////////////////////////////////////////////////////////////////////////////

template<class T>
unique_ptr<T> make_unique_default_init ( ) {
    return make_unique<T> ( );
}
template<class T>
unique_ptr<T> make_unique_default_init ( std::size_t size ) {
    return make_unique<T> ( size );
}
template<class T, class... Args>
typename detail::_Unique_if<T>::_Known_bound make_unique_default_init ( Args &&... ) = delete;

/*

////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct offset_ptr {
    public:
    using value_type    = T;
    using pointer       = value_type *;
    using const_pointer = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type   = std::uint32_t;
    using offset_type = std::uint16_t;

    // Constructors.

    explicit offset_ptr ( ) noexcept : offset ( base.incr_ref_count ( 0u ) ) {}

    explicit offset_ptr ( std::nullptr_t ) : offset ( base.incr_ref_count ( 0u ) ) {}

    explicit offset_ptr ( offset_ptr const & ) noexcept = delete;

    offset_ptr ( offset_ptr && moving ) noexcept { moving.swap ( *this ); }

    template<typename U>
    explicit offset_ptr ( offset_ptr<U> && moving ) noexcept {
        offset_ptr<T> tmp ( moving.release ( ) );
        tmp.swap ( *this );
    }

    offset_ptr ( pointer p_ ) noexcept : offset ( base.incr_ref_count ( p_ - offset_ptr::base.ptr ) ) { assert ( get ( ) == p_ ); }

    // Destruct.

    ~offset_ptr ( ) noexcept {
        base.decr_ref_count ( );
        if ( is_unique ( ) )
            delete get ( );
    }

    // Assignment.

    [[maybe_unused]] offset_ptr & operator= ( std::nullptr_t ) {
        base.incr_ref_count ( );
        reset ( );
        return *this;
    }

    [[maybe_unused]] offset_ptr & operator= ( offset_ptr const & ) noexcept = delete;

    [[maybe_unused]] offset_ptr & operator= ( offset_ptr && moving ) noexcept {
        moving.swap ( *this );
        return *this;
    }

    template<typename U>
    [[maybe_unused]] offset_ptr & operator= ( offset_ptr<U> && moving ) noexcept {
        offset_ptr<T> tmp ( moving.release ( ) );
        tmp.swap ( *this );
        return *this;
    }

    [[maybe_unused]] offset_ptr & operator= ( pointer p_ ) noexcept {
        offset = static_cast<offset_type> ( p_ - offset_ptr::base.ptr );
        assert ( get ( ) == p_ );
        return *this;
    }

    // Get.

    [[nodiscard]] const_pointer operator-> ( ) const noexcept { return get ( ); }
    [[nodiscard]] pointer operator-> ( ) noexcept { return const_cast<pointer> ( std::as_const ( *this ).get ( ) ); }

    [[nodiscard]] const_reference operator* ( ) const noexcept { return *( this->operator-> ( ) ); }
    [[nodiscard]] reference operator* ( ) noexcept { return *( this->operator-> ( ) ); }

    [[nodiscard]] pointer get ( ) const noexcept { return offset_ptr::base.ptr + offset_view ( offset ); }
    [[nodiscard]] pointer get ( offset_type o_ ) const noexcept { return offset_ptr::base + o_; }

    [[nodiscard]] size_type max_size ( ) noexcept {
        return static_cast<size_type> ( std::numeric_limits<offset_type>::max ( ) ) >> 1;
    }

    void swap ( offset_ptr & src ) noexcept { std::swap ( offset, src.offset ); }

    // Other.

    [[nodiscard]] pointer release ( ) noexcept {
        offset_type result = { };
        std::swap ( result, offset );
        return get ( result );
    }

    void reset ( ) noexcept { delete release ( ); }
    void reset ( pointer p_ = pointer ( ) ) noexcept {
        offset_type result = p_ - offset_ptr::base.ptr;
        std::swap ( result, offset );
        delete get ( result );
    }
    template<typename U>
    void reset ( offset_ptr<U> && moving_ ) noexcept {
        offset_ptr<T> result ( moving_ );
        std::swap ( result, *this );
        delete result.get ( );
    }

    void weakify ( ) noexcept { offset = ( offset_view ( offset ) | weak_mask ); }
    void uniquify ( ) noexcept { offset &= offset_mask; }

    [[nodiscard]] bool is_weak ( ) const noexcept { return static_cast<bool> ( offset & weak_mask ); }
    [[nodiscard]] bool is_unique ( ) const noexcept { return not is_weak ( ); }

    [[nodiscard]] static constexpr offset_type offset_view ( offset_type o_ ) noexcept { return o_ & offset_mask; }

    private:
    [[nodiscard]] static constexpr offset_type make_weak_mask ( ) noexcept {
        return static_cast<offset_type> ( std::uint64_t ( 1 ) << ( sizeof ( size_type ) * 8 - 1 ) );
    }
    [[nodiscard]] static constexpr offset_type make_offset_mask ( ) noexcept { return ~make_weak_mask ( ); }

    static constexpr offset_type weak_mask   = offset_ptr::make_weak_mask ( );
    static constexpr offset_type offset_mask = offset_ptr::make_offset_mask ( );

    struct offset_base {

        pointer ptr             = nullptr;
        std::intptr_t ref_count = 0;

        public:
        template<typename U>
        [[maybe_unused]] U && incr_ref_count ( U && o_ ) noexcept {
            if ( ref_count ) {
                ++ref_count;
            }
            else {
                ptr       = stack_top ( );
                ref_count = 1;
            }
            return std::move ( o_ );
        }
        template<typename... Args>
        [[maybe_unused]] auto incr_ref_count ( pointer p_, Args &&... args_ ) noexcept {
            if ( ref_count ) {
                ++ref_count;
            }
            else {
                ptr       = p_;
                ref_count = 1;
            }
            return std::move ( args_... );
        }

        void incr_ref_count ( ) noexcept { incr_ref_count ( 0u ); }
        void decr_ref_count ( ) noexcept { --ref_count; }

        [[nodiscard]] static pointer stack_top ( ) noexcept {
            volatile void * p = std::addressof ( p );
            return reinterpret_cast<pointer> ( const_cast<void *> ( p ) );
        }
    };

    offset_type offset = { };

    static thread_local offset_base base;
};

template<typename T>
thread_local typename offset_ptr<T>::offset_base offset_ptr<T>::base;

*/

////////////////////////////////////////////////////////////////////////////////
/*
template<typename Type>
struct offset_ptr {
    public:
    using value_type    = Type;
    using pointer       = value_type *;
    using const_pointer = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type   = std::uint32_t;
    using offset_type = std::uint16_t;

    // Constructors.

    explicit offset_ptr ( ) noexcept : offset ( offset_ptr::base.incr_ref_count ( ) ) {}

    explicit offset_ptr ( std::nullptr_t ) : offset ( offset_ptr::base.incr_ref_count ( ) ) {}

    explicit offset_ptr ( offset_ptr const & ) noexcept = delete;

    offset_ptr ( offset_ptr && moving ) noexcept { moving.swap ( *this ); }

    template<typename U>
    explicit offset_ptr ( offset_ptr<U> && moving ) noexcept {
        offset_ptr<value_type> tmp ( moving.release ( ) );
        tmp.swap ( *this );
    }

    offset_ptr ( pointer p_ ) noexcept : offset ( offset_ptr::base.incr_ref_count ( p_ ) ) { assert ( get ( ) == p_ ); }

    // Destruct.

    ~offset_ptr ( ) noexcept {
        offset_ptr::base.decr_ref_count ( );
        if ( is_unique ( ) )
            delete get ( );
    }

    // Assignment.

    [[maybe_unused]] offset_ptr & operator= ( std::nullptr_t ) {
        offset_ptr::base.incr_ref_count ( );
        reset ( );
        return *this;
    }

    [[maybe_unused]] offset_ptr & operator= ( offset_ptr const & ) noexcept = delete;

    [[maybe_unused]] offset_ptr & operator= ( offset_ptr && moving ) noexcept {
        moving.swap ( *this );
        return *this;
    }

    template<typename U>
    [[maybe_unused]] offset_ptr & operator= ( offset_ptr<U> && moving ) noexcept {
        offset_ptr<value_type> tmp ( moving.release ( ) );
        tmp.swap ( *this );
        return *this;
    }

    [[maybe_unused]] offset_ptr & operator= ( pointer p_ ) noexcept {
        offset = static_cast<offset_type> ( p_ - offset_ptr::base.get ( ) );
        assert ( get ( ) == p_ );
        return *this;
    }

    // Get.

    [[nodiscard]] const_pointer operator-> ( ) const noexcept { return get ( ); }
    [[nodiscard]] pointer operator-> ( ) noexcept { return const_cast<pointer> ( std::as_const ( *this ).get ( ) ); }

    [[nodiscard]] const_reference operator* ( ) const noexcept { return *( this->operator-> ( ) ); }
    [[nodiscard]] reference operator* ( ) noexcept { return *( this->operator-> ( ) ); }

    [[nodiscard]] pointer get ( ) const noexcept { return offset_ptr::base.get ( ) + offset_view ( offset ); }
    [[nodiscard]] pointer get ( offset_type o_ ) const noexcept { return offset_ptr::base.get ( ) + o_; }

    [[nodiscard]] static size_type max_size ( ) noexcept {
        return static_cast<size_type> ( std::numeric_limits<offset_type>::max ( ) ) >> 1;
    }

    void swap ( offset_ptr & src ) noexcept { std::swap ( offset, src.offset ); }

    // Other.

    [[nodiscard]] pointer release ( ) noexcept {
        offset_type result = { };
        std::swap ( result, offset );
        return get ( result );
    }

    void reset ( ) noexcept { delete release ( ); }
    void reset ( pointer p_ = pointer ( ) ) noexcept {
        offset_type result = p_ - offset_ptr::base.get ( );
        std::swap ( result, offset );
        delete get ( result );
    }
    template<typename U>
    void reset ( offset_ptr<U> && moving_ ) noexcept {
        offset_ptr<value_type> result ( moving_ );
        std::swap ( result, *this );
        delete result.get ( );
    }

    void weakify ( ) noexcept { offset = ( offset_view ( offset ) | weak_mask ); }
    void uniquify ( ) noexcept { offset &= offset_mask; }

    [[nodiscard]] bool is_weak ( ) const noexcept { return static_cast<bool> ( offset & weak_mask ); }
    [[nodiscard]] bool is_unique ( ) const noexcept { return not is_weak ( ); }

    [[nodiscard]] static constexpr offset_type offset_view ( offset_type o_ ) noexcept { return o_ & offset_mask; }

    private:
    [[nodiscard]] static constexpr offset_type make_weak_mask ( ) noexcept {
        return static_cast<offset_type> ( std::uint64_t ( 1 ) << ( sizeof ( size_type ) * 8 - 1 ) );
    }
    [[nodiscard]] static constexpr offset_type make_offset_mask ( ) noexcept { return ~make_weak_mask ( ); }

    static constexpr offset_type weak_mask   = offset_ptr::make_weak_mask ( );
    static constexpr offset_type offset_mask = offset_ptr::make_offset_mask ( );

    class offset_base {

        struct offset_base_data {
            pointer ptr;
            std::intptr_t ref_count = 0;
        };

        std::experimental::fixed_capacity_vector<offset_base_data, 8> seg;

        public:
        [[maybe_unused]] offset_type incr_ref_count ( pointer p_ = nullptr ) {
            if ( offset_base_data * p = get_base_data_ptr ( p_ ); p ) {
                if ( p->ref_count ) {
                    ++p->ref_count;
                    return p_ - p->ptr;
                }
                else
                    *p = { p_, std::intptr_t{ 1 } };
            }
            else {
                if ( seg.size ( ) < 8ull ) {
                    seg.emplace_back ( offset_base_data{ p_, std::intptr_t{ 1 } } );
                    std::sort ( std::begin ( seg ), std::end ( seg ),
                                [] ( offset_base_data const & a, offset_base_data const & b ) noexcept { return a.ptr < b.ptr; } );
                }
                else {
                    auto it = std::find_if ( std::begin ( seg ), std::end ( seg ),
                                             [] ( offset_base_data const & a ) { return not a.ref_count; } );
                    if ( std::end ( seg ) == it )
                        *it = { p_, std::intptr_t{ 1 } };
                    else
                        throw std::runtime_error ( "maximum number of segments exceeded" );
                }
            }
            return 0;
        }

        void decr_ref_count ( ) noexcept { --get_base_data_ptr ( )->ref_count; }

        const_pointer get ( ) const noexcept { return get_base_data_ptr ( )->ptr; }
        pointer get ( ) noexcept { return get_base_data_ptr ( )->ptr; }

        private:
        offset_base_data * get_base_data_ptr ( pointer p_ = nullptr ) const noexcept {
            offset_base_data const * p = nullptr;
            if ( p_ ) {
                for ( offset_base_data const & d : seg ) {
                    if ( d.ptr > p_ )
                        break;
                    if ( p_ < ( d.ptr + max_size ( ) ) ) {
                        p = std::addressof ( d );
                        break;
                    }
                }
            }
            return const_cast<offset_base_data *> ( p );
        }
    };

    offset_type offset = { };

    static thread_local offset_base base;
};

template<typename Type>
thread_local typename offset_ptr<Type>::offset_base offset_ptr<Type>::base;
*/

extern unsigned long __declspec( dllimport ) __stdcall GetProcessHeaps ( unsigned long NumberOfHeaps, void ** ProcessHeaps );

namespace win {

inline std::vector<void *> heaps ( ) noexcept {
    void * h[ 16 ];
    size_t s;
    do {
        s = GetProcessHeaps ( 0, NULL );
    } while ( GetProcessHeaps ( s, h ) != s );
    return std::vector ( h, h + s );
}

inline void * heap ( ) noexcept { return GetProcessHeap ( ); }

inline void * stack ( ) noexcept {
    volatile void * p = std::addressof ( p );
    return const_cast<void *> ( p );
}
} // namespace win

struct offset_ptr_heap_pointer {};
struct offset_ptr_stack_pointer {};

template<typename Type, typename Where>
struct offset_ptr { // offset against this pointer.
    public:
    using value_type    = Type;
    using pointer       = value_type *;
    using const_pointer = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type   = std::uint64_t;
    using offset_type = std::uint16_t;

    // Constructors.

    explicit offset_ptr ( ) noexcept : offset ( 0 ) {}

    explicit offset_ptr ( std::nullptr_t ) : offset ( 0 ) {}

    explicit offset_ptr ( offset_ptr const & ) noexcept = delete;

    offset_ptr ( offset_ptr && moving ) noexcept { moving.swap ( *this ); }

    template<typename U, typename W>
    explicit offset_ptr ( offset_ptr<U, W> && moving ) noexcept {
        offset_ptr<value_type, Where> tmp ( moving.release ( ) );
        tmp.swap ( *this );
    }

    offset_ptr ( pointer p_ ) noexcept : offset ( offset_from_ptr ( p_ ) ) { assert ( get ( ) == p_ ); }

    // Destruct.

    ~offset_ptr ( ) noexcept {
        if constexpr ( not std::is_scalar<Type>::value ) {
            if ( is_unique ( ) )
                delete get ( );
        }
    }

    // Assignment.

    [[maybe_unused]] offset_ptr & operator= ( std::nullptr_t ) {
        reset ( );
        return *this;
    }

    [[maybe_unused]] offset_ptr & operator= ( offset_ptr const & ) noexcept = delete;

    [[maybe_unused]] offset_ptr & operator= ( offset_ptr && moving ) noexcept {
        moving.swap ( *this );
        return *this;
    }

    template<typename U, typename W>
    [[maybe_unused]] offset_ptr & operator= ( offset_ptr<U, W> && moving ) noexcept {
        offset_ptr<Type, Where> tmp ( moving.release ( ) );
        tmp.swap ( *this );
        return *this;
    }

    [[maybe_unused]] offset_ptr & operator= ( pointer p_ ) noexcept {
        offset = offset_from_pointer ( p_ );
        assert ( get ( ) == p_ );
        return *this;
    }

    // Get.

    [[nodiscard]] const_pointer operator-> ( ) const noexcept { return get ( ); }
    [[nodiscard]] pointer operator-> ( ) noexcept { return get ( ); }

    [[nodiscard]] const_reference operator* ( ) const noexcept { return *get ( ); }
    [[nodiscard]] reference operator* ( ) noexcept { return *get ( ); }

    [[nodiscard]] pointer get ( ) const noexcept { return offset_ptr::ptr_from_offset ( offset_view ( offset ) ); }
    [[nodiscard]] pointer get ( ) noexcept { return std::as_const ( *this ).get ( ); }

    [[nodiscard]] static pointer get ( offset_type offset_ ) noexcept { return ptr_from_offset ( offset_view ( offset_ ) ); }

    [[nodiscard]] static size_type max_size ( ) noexcept {
        return static_cast<size_type> ( std::numeric_limits<offset_type>::max ( ) ) >> 1;
    }

    void swap ( offset_ptr & src ) noexcept { std::swap ( *this, src ); }

    // Other.

    [[nodiscard]] pointer release ( ) noexcept {
        offset_type result = { };
        std::swap ( result, offset );
        return get ( result );
    }

    void reset ( ) noexcept {
        if constexpr ( not std::is_scalar<Type>::value ) {
            if ( is_unique ( ) )
                delete release ( );
        }
    }
    void reset ( pointer p_ = pointer ( ) ) noexcept {
        offset_type result = offset_from_ptr ( p_ );
        std::swap ( result, offset );
        if constexpr ( not std::is_scalar<Type>::value ) {
            if ( is_unique ( ) )
                delete get ( result );
        }
    }
    template<typename U, typename W>
    void reset ( offset_ptr<U, W> && moving_ ) noexcept {
        offset_ptr<Type, Where> result ( moving_ );
        std::swap ( result, *this );
        if constexpr ( not std::is_scalar<Type>::value ) {
            if ( is_unique ( ) )
                delete result.get ( );
        }
    }

    void weakify ( ) noexcept { offset = ( offset_ptr::offset_view ( offset ) | weak_mask ); }
    void uniquify ( ) noexcept { offset &= offset_mask; }

    [[nodiscard]] bool is_weak ( ) const noexcept { return static_cast<bool> ( offset & weak_mask ); }
    [[nodiscard]] bool is_unique ( ) const noexcept { return not is_weak ( ); }

    private:
    offset_type offset = { };

    [[nodiscard]] pointer addressof_this ( ) const noexcept {
        return reinterpret_cast<pointer> ( const_cast<offset_ptr *> ( this ) );
    }

    // Static class functions and variables.

    [[nodiscard]] static constexpr offset_type offset_view ( offset_type o_ ) noexcept { return o_ & offset_mask; }

    [[nodiscard]] static offset_type offset_from_ptr ( pointer p_ ) noexcept {
        return static_cast<offset_type> ( p_ - offset_ptr::base );
    }
    [[nodiscard]] static pointer ptr_from_offset ( offset_type const offset_ ) noexcept {
        return offset_ptr::base + offset_ptr::offset_view ( offset_ );
    }

    [[nodiscard]] static constexpr offset_type make_weak_mask ( ) noexcept {
        return static_cast<offset_type> ( std::uint64_t ( 1 ) << ( sizeof ( size_type ) * 8 - 1 ) );
    }
    [[nodiscard]] static constexpr offset_type make_offset_mask ( ) noexcept { return ~make_weak_mask ( ); }

    static constexpr offset_type weak_mask   = offset_ptr::make_weak_mask ( );
    static constexpr offset_type offset_mask = offset_ptr::make_offset_mask ( );

    [[nodiscard]] static constexpr pointer mask_low ( pointer p_ ) noexcept {
        return reinterpret_cast<pointer> ( ( reinterpret_cast<std::uintptr_t> ( p_ ) & 0xFFFF'FFFF'0000'0000 ) );
    }

    [[nodiscard]] static int pointer_alignment ( void * ptr_ ) noexcept {
        return ( int ) ( ( ( std::uintptr_t ) ptr_ ) & ( ( std::uintptr_t ) ( -( ( std::intptr_t ) ptr_ ) ) ) );
    }

    [[nodiscard]] static pointer base_pointer ( ) noexcept {
        if constexpr ( std::is_same<Where, offset_ptr_heap_pointer>::value ) {
            return static_cast<pointer> ( win::heap ( ) );
        }
        else {
            return static_cast<pointer> ( win::stack ( ) );
        }
    }

    static thread_local pointer base;
};

template<typename Type, typename Where>
thread_local typename offset_ptr<Type, Where>::pointer offset_ptr<Type, Where>::base = offset_ptr::base_pointer ( );

template<typename Type>
using heap_offset_ptr = offset_ptr<Type, offset_ptr_heap_pointer>;

template<typename Type>
using stack_offset_ptr = offset_ptr<Type, offset_ptr_stack_pointer>;
