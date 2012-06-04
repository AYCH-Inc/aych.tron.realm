/*************************************************************************
 *
 * TIGHTDB CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2012] TightDB Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of TightDB Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to TightDB Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from TightDB Incorporated.
 *
 **************************************************************************/
#ifndef TIGHTDB_TUPLE_HPP
#define TIGHTDB_TUPLE_HPP

#include <ostream>

#include "type_list.hpp"

namespace tightdb {


template<class L> struct Tuple {
    typedef typename L::head head_type;
    typedef Tuple<typename L::tail> tail_type;
    head_type m_head;
    tail_type m_tail;
    Tuple(const head_type& h, const tail_type& t): m_head(h), m_tail(t) {}
};
template<> struct Tuple<void> {};


inline Tuple<void> tuple() { return Tuple<void>(); }

template<class T> inline Tuple<TypeCons<T, void> > tuple(const T& v)
{
    return Tuple<TypeCons<T, void> >(v, tuple());
}

template<class H, class T> inline Tuple<TypeCons<H,T> > cons(const H& h, const Tuple<T>& t)
{
    return Tuple<TypeCons<H,T> >(h,t);
}


template<class L, class V>
inline Tuple<typename TypeAppend<L,V>::type> append(const Tuple<L>& t, const V& v)
{
    return cons(t.m_head, append(t.m_tail, v));
}
template<class V>
inline Tuple<TypeCons<V, void> > append(const Tuple<void>&, const V& v)
{
    return tuple(v);
}
template<class L, class V>
inline Tuple<typename TypeAppend<L,V>::type> operator,(const Tuple<L>& t, const V& v)
{
    return append(t,v);
}

namespace _impl {
    template<class L, int i> struct TupleAt {
        static typename TypeAt<L,i>::type exec(const Tuple<L>& t)
        {
            return TupleAt<typename L::tail, i-1>::exec(t.m_tail);
        }
    };
    template<class L> struct TupleAt<L,0> {
        static typename L::head exec(const Tuple<L>& t) { return t.m_head; }
    };

    template<class Ch, class Tr, class T>
    inline void write(std::basic_ostream<Ch, Tr>& out, const Tuple<TypeCons<T, void> >& t)
    {
        out << t.m_head;
    }
    template<class Ch, class Tr>
    inline void write(std::basic_ostream<Ch, Tr>&, const Tuple<void>&) {}
    template<class Ch, class Tr, class L>
    inline void write(std::basic_ostream<Ch, Tr>& out, const Tuple<L>& t)
    {
        out << t.m_head << ',';
        write(out, t.m_tail);
    }
}

template<int i, class L> typename TypeAt<L,i>::type at(const Tuple<L>& tuple)
{
    return _impl::TupleAt<L,i>::exec(tuple);
}

template<class Ch, class Tr, class L>
std::basic_ostream<Ch, Tr>& operator<<(std::basic_ostream<Ch, Tr>& out, const Tuple<L>& t)
{
    out << '(';
    _impl::write(out, t);
    out << ')';
    return out;
}


} // namespace tightdb

#endif // TIGHTDB_TUPLE_HPP
