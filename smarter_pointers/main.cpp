
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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <array>
#include <sax/iostream.hpp>
#include <iterator>
#include <list>
#include <map>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include <sax/splitmix.hpp>
#include <sax/uniform_int_distribution.hpp>

#include <offset_ptr.hpp>

// -fsanitize=address
/*
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan_cxx-x86_64.lib;
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan-preinit-x86_64.lib;
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan-x86_64.lib
*/

template<typename Key, typename Value, size_t Capacity>
struct simple_map {

    public:
    using value_type     = Value;
    using key_type       = Key;
    using key_value_type = std::pair<key_type, value_type>;

    using pointer       = key_value_type *;
    using const_pointer = key_value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type       = std::size_t;
    using difference_type = std::make_signed<size_type>;

    using iterator               = pointer;
    using const_iterator         = const_pointer;
    using reverse_iterator       = pointer;
    using const_reverse_iterator = const_pointer;

    void clear ( ) noexcept {
        if constexpr ( not std::is_scalar<Value>::value ) {
            for ( auto & v : *this )
                v = { { }, {} };
        }
        m_end = begin ( );
    }

    [[nodiscard]] static constexpr size_type max_size ( ) noexcept { return Capacity; }
    [[nodiscard]] static constexpr size_type capacity ( ) noexcept { return Capacity; }

    [[nodiscard]] size_type size ( ) const noexcept { return m_end - data ( ); }

    template<class M>
    std::pair<iterator, bool> insert_or_assign ( key_type && key_, M && value_ ) noexcept {
        auto it = std::lower_bound ( begin ( ), end ( ), key_, map_comaparator<key_value_type> ( ) );
        if ( it->first == key_ ) {
            it->second = std::move ( value_ );
            return { std::move ( it ), false };
        }
        else if ( size ( ) < capacity ( ) ) {
            ++m_end;
            for ( auto rit = rbegin ( ); it != rit; ++rit )
                *rit = *std::next ( rit );
            *it = { std::move ( key_ ), std::move ( value_ ) };
            return { it, true };
        }
        else {
            return { nullptr, false };
        }
    }

    template<typename Pair>
    struct map_comaparator {
        bool operator( ) ( Pair const & a, Pair const & b ) const noexcept { return a.first < b.first; }
    };

    iterator binary_find ( key_type const & key_ ) const noexcept {
        auto first = std::lower_bound ( begin ( ), end ( ), key_, map_comaparator<key_value_type> ( ) );
        return first != end ( ) and not map_comaparator<key_value_type> ( key_, *first ) ? first : end ( );
    }

    iterator linear_lowerbound ( key_type const & key ) const noexcept {
        for ( key_value_type const & kv : *this )
            if ( kv.first >= key )
                return std::addressof ( kv );
        return end ( );
    };

    iterator linear_find ( key_type const & key_ ) const noexcept {
        auto first = linear_lowerbound ( key_ );
        return first != end ( ) and key_ == *first ? first : end ( );
    }

    iterator find ( value_type const & val_ ) const noexcept {
        for ( key_value_type const & kv : *this )
            if ( kv.second == val_ )
                return std::addressof ( kv );
        return end ( );
    }

    [[nodiscard]] const_pointer data ( ) const noexcept { return m_data.data ( ); }
    [[nodiscard]] pointer data ( ) noexcept { return const_cast<pointer> ( std::as_const ( *this ).data ( ) ); }

    // Iterators.

    [[nodiscard]] const_iterator begin ( ) const noexcept { return m_data.data ( ); }
    [[nodiscard]] const_iterator cbegin ( ) const noexcept { return begin ( ); }
    [[nodiscard]] iterator begin ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).begin ( ) ); }

    [[nodiscard]] const_iterator end ( ) const noexcept { return m_end; }
    [[nodiscard]] const_iterator cend ( ) const noexcept { return end ( ); }
    [[nodiscard]] iterator end ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).end ( ) ); }

    [[nodiscard]] const_iterator rbegin ( ) const noexcept { return m_end - 1; }
    [[nodiscard]] const_iterator crbegin ( ) const noexcept { return rbegin ( ); }
    [[nodiscard]] iterator rbegin ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).rbegin ( ) ); }

    [[nodiscard]] const_iterator rend ( ) const noexcept { return m_data.data ( ) - 1; }
    [[nodiscard]] const_iterator crend ( ) const noexcept { return rend ( ); }
    [[nodiscard]] iterator rend ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).rend ( ) ); }

    [[nodiscard]] const_reference front ( ) const noexcept { return *m_data.data ( ); }
    [[nodiscard]] reference front ( ) noexcept { return const_cast<reference> ( std::as_const ( *this ).front ( ) ); }

    [[nodiscard]] const_reference back ( ) const noexcept { return *( m_end - 1 ); }
    [[nodiscard]] reference back ( ) noexcept { return const_cast<reference> ( std::as_const ( *this ).back ( ) ); }

    [[nodiscard]] const_reference at ( size_type const i_ ) const {
        if ( 0 <= i_ and i_ < size ( ) )
            return m_data[ i_ ];
        else
            throw std::runtime_error ( "simple_map: index out of bounds" );
    }
    [[nodiscard]] reference at ( size_type const i_ ) { return const_cast<reference> ( std::as_const ( *this ).at ( i_ ) ); }

    [[nodiscard]] const_reference operator[] ( size_type const i_ ) const noexcept { return m_data[ i_ ]; }
    [[nodiscard]] reference operator[] ( size_type const i_ ) noexcept {
        return const_cast<reference> ( std::as_const ( *this ).operator[] ( i_ ) );
    }

    std::array<key_value_type, Capacity> m_data;
    pointer m_end = begin ( );
};

void handleEptr ( std::exception_ptr eptr ) { // Passing by value is ok.
    try {
        if ( eptr )
            std::rethrow_exception ( eptr );
    }
    catch ( std::exception const & e ) {
        std::cout << "Caught exception \"" << e.what ( ) << "\"\n";
    }
}

int main ( ) {

    std::exception_ptr eptr;

    try {

        simple_map<void *, std::size_t, 8> m;

        std::cout << "simple_map " << sizeof ( m ) << " " << m.size ( ) << nl;

        sax::heap_offset_ptr<int> p0 ( ( int * ) std::malloc ( 8 * sizeof ( int ) ) );

        p0.weakify ( );

        std::cout << "leaving try block " << sizeof ( p0 ) << " " << sax::detail::win::heaps ( ).size ( ) << nl;
    }
    catch ( ... ) {
        eptr = std::current_exception ( ); // Capture.
    }
    handleEptr ( eptr );

    std::cout << "leaving main" << nl;

    return EXIT_SUCCESS;
}

/*

int a1[] = { 1, 2, 3 };
int a2[] = { 10, 11, 12 };
int * based;

typedef int __based ( based ) * pBasedPtr;

using namespace std;
int main ( ) {
    based        = &a1[ 0 ];
    pBasedPtr pb = 0;

    cout << pb << endl;
    cout << ( pb + 1 ) << endl;

    based = &a2[ 0 ];

    cout << *pb << endl;
    cout << *( pb + 1 ) << endl;
}

*/
