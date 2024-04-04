#pragma once

#include <cmath>
#include <cstdint>


namespace H2G {

template<typename NUM>
struct NumTraits {
    using NumType    = NUM;
    using ExtendType = NumType;
    using FloatType  = NumType;
};

template<>
struct NumTraits<int> {
    using NumType    = int;
    using ExtendType = int64_t;
    using FloatType  = double;
};

template<typename NUM>
struct Point {
	using ExtNum = typename NumTraits<NUM>::ExtendType;
    NUM m_x, m_y;

    constexpr Point(NUM x, NUM y): m_x(x), m_y(y) {}

    constexpr Point operator-(const Point& oth) const { return Point(m_x - oth.m_x, m_y - oth.m_y); }
    constexpr Point operator+(const Point& oth) const { return Point(m_x + oth.m_x, m_y + oth.m_y); }

    NUM    EuclideanNorm() const { return NUM(std::sqrt(SquaredEuclideanNorm())); }
    constexpr ExtNum SquaredEuclideanNorm() const { return static_cast<ExtNum>(m_x) * m_x + static_cast<ExtNum>(m_y) * m_y; }
    ExtNum Dot(const Point& oth) const { return static_cast<ExtNum>(m_x) * oth.m_x + static_cast<ExtNum>(m_y) * oth.m_y; }
    constexpr ExtNum Cross(const Point& oth) const { return static_cast<ExtNum>(m_x) * oth.m_y - static_cast<ExtNum>(m_y) * oth.m_x; }
    Point<NUM> Resize(NUM s) const
    {
        const auto n = EuclideanNorm();
        return Point{NUM(static_cast<ExtNum>(s)*m_x/n), NUM(static_cast<ExtNum>(s)*m_y/n)};
    }
    constexpr Point<NUM> Perpendicular() const
    {
        return Point<NUM>(-m_y, m_x);
    }

    constexpr bool operator==(const Point& oth) const { return m_x == oth.m_x && m_y == oth.m_y; }
    constexpr bool operator!=(const Point& oth) const { return !operator==(oth); }
};

template<typename NUM>
using Vector2 = Point<NUM>;

}



#include <limits>


namespace H2G {

template<typename NUM>
class Box2D {
public:
    constexpr Box2D():
        m_lb(std::numeric_limits<NUM>::max(), std::numeric_limits<NUM>::max()),
        m_rt(std::numeric_limits<NUM>::min(), std::numeric_limits<NUM>::min()) {}

    explicit constexpr Box2D(const Point<NUM>& pt): m_lb(pt), m_rt(pt) {}

    constexpr Box2D merge(const Point<NUM>& pt) const {
        Box2D ans = *this;
        ans.m_lb.m_x = std::min(ans.m_lb.m_x, pt.m_x);
        ans.m_lb.m_y = std::min(ans.m_lb.m_y, pt.m_y);
        ans.m_rt.m_x = std::max(ans.m_rt.m_x, pt.m_x);
        ans.m_rt.m_y = std::max(ans.m_rt.m_y, pt.m_y);
        return ans;
    }

    constexpr Box2D merge(const Box2D& oth) const {
        return merge(oth.m_lb).merge(oth.m_rt);
    }

    constexpr bool contains(const Point<NUM>& pt) const
    {
        return m_lb.m_x <= pt.m_x && pt.m_x <= m_rt.m_x &&
               m_lb.m_y <= pt.m_y && pt.m_y <= m_rt.m_y;
    }

    constexpr bool contains(const Box2D& box) const
    {
        return contains(box.m_lb) && contains(box.m_rt);
    }

    constexpr bool operator==(const Box2D& oth) const { return m_lb == oth.m_lb && m_rt == oth.m_rt; }
    constexpr bool operator!=(const Box2D& oth) const { return !operator==(oth); }

    constexpr NUM width() const { return m_rt.m_x - m_lb.m_x; }
    constexpr NUM height() const { return m_rt.m_y - m_lb.m_y; }

private:
    Point<NUM> m_lb, m_rt;
};

}


#include <utility>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <vector>


namespace H2G {
template<typename T, size_t N, bool appendvector=false>
class maxsize_vector {
private:
    template<bool reverse>
    class const_iterator_impl {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;
        using reference         = T&;
        using const_reference   = const T&;

    protected:
        const maxsize_vector& m_vec;
        size_t m_idx;

    public:
        const_iterator_impl(const maxsize_vector& vec, size_t idx): m_vec(vec), m_idx(idx) {}

        const T& operator*() const {
            assert(m_vec.size() > m_idx);
            if constexpr (reverse) {
                return m_vec.at(m_vec.size() - m_idx - 1);
            } else {
                return m_vec.at(m_idx);
            }
        }

        const_iterator_impl& operator++() {
            assert(this->m_idx < m_vec.size());
            this->m_idx++;
            return *this;
        }

        const_iterator_impl& operator--() {
            assert(this->m_idx > 0);
            this->m_idx--;
            return *this;
        }

        const_iterator_impl operator++(int) { auto ans = *this; this->operator++(); return ans; }
        const_iterator_impl operator--(int) { auto ans = *this; this->operator--(); return ans; }

        const_iterator_impl& operator+(int n) { assert(m_idx + n <= m_vec.size()); m_idx += n; }
        const_iterator_impl& operator-(int n) { assert(m_idx >= n); m_idx -= n; }

        std::ptrdiff_t operator-(const const_iterator_impl& oth) { std::ptrdiff_t idx = m_idx; idx -= oth.m_idx; return idx; }

        bool operator< (const const_iterator_impl& oth) const { assert(&this->m_vec == &oth.m_vec); return this->m_idx <  oth.m_idx; }
        bool operator<=(const const_iterator_impl& oth) const { assert(&this->m_vec == &oth.m_vec); return this->m_idx <= oth.m_idx; }
        bool operator> (const const_iterator_impl& oth) const { assert(&this->m_vec == &oth.m_vec); return this->m_idx >  oth.m_idx; }
        bool operator>=(const const_iterator_impl& oth) const { assert(&this->m_vec == &oth.m_vec); return this->m_idx >= oth.m_idx; }
        bool operator==(const const_iterator_impl& oth) const { assert(&this->m_vec == &oth.m_vec); return this->m_idx == oth.m_idx; }
        bool operator!=(const const_iterator_impl& oth) const { return !this->operator==(oth); }
    };

    template<bool reverse>
    class iterator_impl: public const_iterator_impl<reverse> {
    public:
        using iterator_category = typename const_iterator_impl<reverse>::iterator_category;
        using difference_type   = typename const_iterator_impl<reverse>::difference_type;
        using value_type        = typename const_iterator_impl<reverse>::value_type;
        using pointer           = typename const_iterator_impl<reverse>::pointer;
        using reference         = typename const_iterator_impl<reverse>::reference;
        using const_reference   = typename const_iterator_impl<reverse>::const_reference;

        iterator_impl(maxsize_vector& vec, size_t idx): const_iterator_impl<reverse>(vec, idx) {}

        T& operator*() { return const_cast<T&>(static_cast<const const_iterator_impl<reverse>*>(this)->operator*()); }
    };

public:
    using iterator               = iterator_impl<false>;
    using reverse_iterator       = iterator_impl<true>;
    using const_iterator         = const_iterator_impl<false>;
    using const_reverse_iterator = const_iterator_impl<true>;

    inline size_t size() const { return this->m_size; }

    inline T&       back() {
        if constexpr (appendvector) {
            if (m_size <= N) {
                return m_array[m_size - 1].as();
            } else {
                return m_appvec.back();
            }
        } else {
            return m_array[m_size - 1].as();
        }
    }
    inline const T& back() const { return const_cast<maxsize_vector*>(this)->back(); }
    inline T&       front() {return at(0);}
    inline const T& front() const {return at(0);}

    inline void clear() {
        if constexpr (appendvector) {
            if (m_size > N) {
                this->m_appvec.clear();
                m_size = N;
            }
        }

        if constexpr (!std::is_trivially_destructible_v<T>) {
            while (!this->empty()) this->pop_back();
        } else {
            m_size = 0;
        }
    }
    template<typename K>
    inline void push_back(K&& val) {
        if constexpr (appendvector) {
            if (m_size < N) {
                m_array[m_size++].construct_with(std::forward<K>(val));
            } else {
                m_appvec.push_back(std::forward<K>(val));
                m_size++;
            }
        } else {
            assert(m_size < N);
            m_array[m_size++].construct_with(std::forward<K>(val));
        }
    }
    template<typename ... Args>
    inline void emplace_back(Args&& ... args) {
        if constexpr (appendvector) {
            if (m_size < N) {
                m_array[m_size++].construct_with(std::forward<Args>(args)...);
            } else {
                m_appvec.emplace_back(std::forward<Args>(args)...);
                m_size++;
            }
        } else {
            assert(m_size < N);
            m_array[m_size++].construct_with(std::forward<Args>(args)...);
        }
    }
    inline void pop_back() {
        assert(m_size > 0);
        m_size--;
        if constexpr (appendvector) {
            if (m_size < N) {
                m_array[m_size].destroy();
            } else {
                assert(m_appvec.size() > 0);
                m_appvec.pop_back();
            }
        } else {
            m_array[m_size].destroy();
        }
    }

    inline void swap(maxsize_vector& oth) {
        if constexpr (appendvector) {
            m_appvec.swap(oth.m_appvec);
        }
        const auto m = std::min(std::min(m_size, oth.m_size), N);
        for (size_t i=0;i<m;i++) {
            std::swap(at(i), oth.at(i));
        }
        for (size_t i=m;i<std::min(m_size,N);i++) {
            oth.m_array[i].construct_with(std::move(at(i)));;
            m_array[i].destroy();
        }
        for (size_t i=m;i<std::min(oth.m_size,N);i++) {
            m_array[i].construct_with(std::move(oth.at(i)));;
            oth.m_array[i].destroy();
        }
        std::swap(m_size, oth.m_size);
    }

    inline T&       operator[](size_t idx) { return this->at(idx); }
    inline const T& operator[](size_t idx) const { return this->at(idx); }

    inline T&       at(size_t idx) {
        assert(idx < m_size);
        if constexpr (appendvector) {
            if (idx < N) {
                return m_array[idx].as();
            } else {
                return m_appvec[idx - N];
            }
        } else {
            return m_array[idx].as();
        }
    }
    inline const T& at(size_t idx) const { return const_cast<maxsize_vector*>(this)->at(idx); }

    inline bool empty() const { return m_size == 0; }

    template<typename Iter>
    inline maxsize_vector(Iter begin, Iter end): m_size(0) {
        for (;begin!=end;++begin) {
            this->push_back(*begin);
        }
    }

    inline maxsize_vector(std::initializer_list<T> vec): m_size(0) {
        for (auto v: vec)
            this->push_back(v);
    }

    maxsize_vector(maxsize_vector&& oth): m_size(0)
    {
        for (size_t i=0;i<N && i < oth.size();i++)
            this->emplace_back(std::move(oth.at(i)));
        m_appvec = std::move(oth.m_appvec);
        this->m_size = oth.m_size;
        oth.m_size = 0;
    }

    maxsize_vector(const maxsize_vector& oth): m_size(0)
    {
        for (auto& v: oth) this->emplace_back(v);
    }

    maxsize_vector& operator=(maxsize_vector&& oth) {
        this->clear();
        for (size_t i=0;i<N && i < oth.size();i++)
            this->emplace_back(std::move(oth.at(i)));
        m_appvec = std::move(oth.m_appvec);
        this->m_size = oth.m_size;
        return *this;
    }

    maxsize_vector& operator=(const maxsize_vector& oth) {
        this->clear();
        for (auto& v: oth) this->emplace_back(v);
        return *this;
    }

    bool operator==(const maxsize_vector& oth) const {
        if (this->size() != oth.size()) return false;
        for (size_t i=0;i<this->size();i++) {
            if (this->at(i) != oth.at(i)) return false;
        }
        return true;
    }
    bool operator!=(const maxsize_vector& oth) const { return !this->operator==(oth); }

    iterator       begin()        { return iterator(*this, 0); }
    const_iterator begin()  const { return const_iterator(*this, 0); }
    const_iterator cbegin() const { return const_iterator(*this, 0); }
    iterator       end()        { return iterator(*this, m_size); }
    const_iterator end()  const { return const_iterator(*this, m_size); }
    const_iterator cend() const { return const_iterator(*this, m_size); }

    reverse_iterator       rbegin()        { return reverse_iterator(*this, 0); }
    const_reverse_iterator rbegin()  const { return const_reverse_iterator(*this, 0); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(*this, 0); }
    reverse_iterator       rend()        { return reverse_iterator(*this, m_size); }
    const_reverse_iterator rend()  const { return const_reverse_iterator(*this, m_size); }
    const_reverse_iterator crend() const { return const_reverse_iterator(*this, m_size); }

    inline maxsize_vector(): m_size(0) {}
    inline ~maxsize_vector() { this->clear(); }

private:
    struct DT {
        template<typename ... Args>
        void construct_with(Args&& ... args) { new (m_buf) T(std::forward<Args>(args)...); }

        void destroy() { this->as().~T(); }

        const T& as() const { return const_cast<DT*>(this)->as(); }
        T& as() { return *reinterpret_cast<T*>(static_cast<void*>(m_buf)); }

    private:
        alignas(T) std::byte m_buf[sizeof(T)];
    };

    size_t m_size;
    DT m_array[N];
    struct dummy_struct {};
    std::conditional_t<appendvector,std::vector<T>,dummy_struct> m_appvec;
};

template<typename T, size_t N=10>
using qarray = maxsize_vector<T,N,true>;
};



#include <cassert>
#include <optional>
#include <cstdint>
#include <cmath>
#include <variant>

#define H2GAssert(cond) assert(cond)
#define H2GUnreachable() assert(false)

namespace H2G {

template<typename NUM>
inline bool compare_aob2cod(NUM a, NUM b, NUM c, NUM d)
{
    H2GAssert(b != 0 && d != 0);
    if constexpr (std::is_integral_v<NUM>) {
        const int sa = a > 0 ? 1 : (a == 0 ? 0 : -1);
        const int sb = b > 0 ? 1 : (b == 0 ? 0 : -1);
        const int sc = c > 0 ? 1 : (c == 0 ? 0 : -1);
        const int sd = d > 0 ? 1 : (d == 0 ? 0 : -1);
        if (a == 0) {
            return sc * sd > 0;
        } else if (c == 0) {
            return sa * sb < 0;
        }
        if (sa * sb != sc * sd) {
            return sa * sb < 0;
        }
        const NUM na = a > 0 ? a : -a;
        const NUM nb = b > 0 ? b : -b;
        const NUM nc = c > 0 ? c : -c;
        const NUM nd = d > 0 ? d : -d;
        const NUM k1 = na / nb;
        const NUM k2 = nc / nd;
        const bool tv = sa * sb > 0;
        if (k1 < k2) return tv;
        if (k1 > k2) return !tv;
        const NUM f1 = na % nb;
        const NUM f2 = nc % nd;
        if (f1 == 0) {
            return f2 > 0 ? tv : false;
        } else if (f2 == 0) {
            return !tv;
        }
        if (tv) {
            return compare_aob2cod(nd, f2, nb, f1);
        } else {
            return compare_aob2cod(nb, f1, nd, f2);
        }
    } else {
        return a / b < c / d;
    }
}

template<typename NUM>
struct DAngle {
    using ExtNUM = typename NumTraits<NUM>::ExtendType;
    explicit DAngle(const Point<NUM>& vec): m_x(vec.m_x), m_y(vec.m_y) { }
    DAngle(NUM x, NUM y): m_x(x), m_y(y) {}

    DAngle<ExtNUM> ExtendedAngle() const {
        const auto v1 = static_cast<ExtNUM>(m_x) * static_cast<ExtNUM>(m_x);
        const auto v2 = static_cast<ExtNUM>(m_y) * static_cast<ExtNUM>(m_y);
        const auto v3 = v1 * (m_x > 0 ? 1 : -1);
        const auto v4 = v2 * (m_y > 0 ? 1 : -1);
        return DAngle<ExtNUM>(v3, v4);
    }

    bool operator<(const DAngle& oth) const
    {
        if (m_y >= 0 && oth.m_y < 0) {
            return true;
        } else if (m_y < 0 && oth.m_y >= 0) {
            return false;
        } else if (m_y == 0 && oth.m_y == 0) {
            return m_x >= 0 && oth.m_x < 0;
        } else if (m_y == 0 && oth.m_y > 0) {
            return m_x >= 0;
        } else if (m_y > 0 && oth.m_y == 0) {
            return oth.m_x < 0;
        } else if (m_y > 0 && oth.m_y > 0) {
            if constexpr (std::is_same_v<ExtNUM,NUM>) {
                const auto v1 = compare_aob2cod(oth.m_x, oth.m_y, m_x, m_y);
                return v1;
            } else {
                const auto v2 =  ExtNUM(m_x) * oth.m_y > ExtNUM(oth.m_x) * m_y;
                return v2;
            }
        } else if (m_y < 0 && oth.m_y < 0) {
            if constexpr (std::is_same_v<ExtNUM,NUM>) {
                const auto v1 = compare_aob2cod(oth.m_x, oth.m_y, m_x, m_y);
                return v1;
            } else {
                const auto v2 =  ExtNUM(m_x) * oth.m_y > ExtNUM(oth.m_x) * m_y;
                return v2;
            }
        } else {
            H2GUnreachable();
        }
    }

    static DAngle Angle0() { return DAngle(1, 0); }
    static DAngle Angle90() { return DAngle(0, 1); }
    static DAngle Angle180() { return DAngle(-1, 0); }
    static DAngle Angle270() { return DAngle(0, -1); }

    bool operator>(const DAngle& oth) const { return oth.operator<(*this); }
    bool operator<=(const DAngle& oth) const { return !operator>(oth); }
    bool operator>=(const DAngle& oth) const { return !operator<(oth); }
    bool operator==(const DAngle& oth) const
    {
        const Point<NUM> p1(m_x, m_y);
        const Point<NUM> p2(oth.m_x, oth.m_y);
        return p1 == p2 || (p1.Cross(p2) == 0 && p1.Dot(p2) > 0);
    }

    NUM m_x, m_y;
};

template<typename NUM>
struct DAngleRange {
    DAngleRange(DAngle<NUM> from, DAngle<NUM> to, bool cclockwise):
        m_from(from), m_to(to), m_cclockwise(cclockwise) {}

    bool contains(DAngle<NUM> angle) const
    {
        if (m_from == m_to) {
            return true;
        } else if (m_from < m_to) {
            if (m_cclockwise) {
                return m_from <= angle && angle <= m_to;
            } else {
                return angle >= m_to || angle <= m_from;
            }
        } else {
            if (m_cclockwise) {
                return angle >= m_from || angle <= m_to;
            } else {
                return m_to <= angle && angle <= m_from;
            }
        }
    }

    bool operator==(const DAngleRange& oth) const {
        return m_from == oth.m_from && m_to == oth.m_to && m_cclockwise == oth.m_cclockwise;
    }
    bool operator!=(const DAngleRange& oth) const { return !operator==(oth); }

    DAngle<NUM> m_from, m_to;
    bool m_cclockwise;
};


template<typename NUM>
Point<NUM> nearestPointSegment(Point<NUM> A, Point<NUM> B, Point<NUM> P)
{
	using ExtNum = typename NumTraits<NUM>::ExtendType;
    using FltNum = typename NumTraits<NUM>::FloatType;
    if (A == B) return A;
    const Point<NUM> AB(B.m_x - A.m_x, B.m_y - A.m_y);
    const Point<NUM> AP(P.m_x - A.m_x, P.m_y - A.m_y);
    
    const ExtNum dotProductABAP =  AB.Dot(AP);
    const ExtNum lengthSquaredAB = AB.SquaredEuclideanNorm();
    
    if (dotProductABAP >= 0 && dotProductABAP <= lengthSquaredAB) {
        const FltNum lambda = static_cast<FltNum>(dotProductABAP) / lengthSquaredAB;
        return Point<NUM>(A.m_x + lambda * AB.m_x, A.m_y + lambda * AB.m_y);
    } else {
        double distanceToA = (P - A).SquaredEuclideanNorm();
        double distanceToB = (P - B).SquaredEuclideanNorm();
        
        if (distanceToA < distanceToB) {
            return A;
        } else {
            return B;
        }
    }
}

template<typename NUM>
std::optional<Point<NUM>> lineSegmentsIntersect(Point<NUM> A, Point<NUM> B, Point<NUM> C, Point<NUM> D)
{
	using ExtNum = typename NumTraits<NUM>::ExtendType;
    using FltNum = typename NumTraits<NUM>::FloatType;

    // Line AB represented as a1x + b1y = c1
    const ExtNum a1 = B.m_y - A.m_y;
    const ExtNum b1 = A.m_x - B.m_x;
    const ExtNum c1 = a1 * (A.m_x) + b1 * (A.m_y);

    // Line CD represented as a2x + b2y = c2
    const ExtNum a2 = D.m_y - C.m_y;
    const ExtNum b2 = C.m_x - D.m_x;
    const ExtNum c2 = a2 * (C.m_x) + b2 * (C.m_y);

    const FltNum determinant = a1 * b2 - a2 * b1;

    if (determinant == 0) {
        return std::nullopt;
    } else {
        const FltNum x = (b2 * c1 - b1 * c2) / determinant;
        const FltNum y = (a1 * c2 - a2 * c1) / determinant;

        // Check if the intersection point is on both line segments
        if (x < std::min(A.m_x, B.m_x) || x > std::max(A.m_x, B.m_x) ||
            x < std::min(C.m_x, D.m_x) || x > std::max(C.m_x, D.m_x) ||
            y < std::min(A.m_y, B.m_y) || y > std::max(A.m_y, B.m_y) ||
            y < std::min(C.m_y, D.m_y) || y > std::max(C.m_y, D.m_y))
        {
            return std::nullopt;
        }

        return Point<NUM>(x, y);
    }
}

enum class SHAPE_TYPE
{
    NONE, SEGMENT, ARC_SEGMENT, CIRCLE, POLYGON, COMPLEX_POLYGON,
};

template<typename NUM>
struct SHAPE_TYPE_DATA {
    SHAPE_TYPE m_type;

    explicit SHAPE_TYPE_DATA(SHAPE_TYPE type): m_type(type) {}
};

template<typename NUM>
struct SEGMENT_DATA: public SHAPE_TYPE_DATA<NUM> {
    Point<NUM> m_a, m_b;

    constexpr SEGMENT_DATA(Point<NUM> a, Point<NUM> b):
        SHAPE_TYPE_DATA<NUM>(SHAPE_TYPE::SEGMENT), m_a(a), m_b(b) {}

    constexpr Box2D<NUM> box() const { return Box2D<NUM>(m_a).merge(m_b); }

    constexpr bool operator==(const SEGMENT_DATA& oth) const { return m_a == oth.m_a && m_b == oth.m_b; }
    constexpr bool operator!=(const SEGMENT_DATA& oth) const { return !operator==(oth); }
};

template<typename NUM>
bool pointInLine(Point<NUM> p1, Point<NUM> p2, Point<NUM> pt)
{
    if (p1 == p2) return p1 == pt;

    const auto vec = (p2 - p1);
    if (std::abs(vec.m_x) > std::abs(vec.m_y)) {
        if (p1.m_x < p2.m_x) {
            return p1.m_x <= pt.m_x && pt.m_x <= p2.m_x;
        } else {
            return p1.m_x >= pt.m_x && pt.m_x >= p2.m_x;
        }
    } else {
        if (p1.m_y < p2.m_y) {
            return p1.m_y <= pt.m_y && pt.m_y <= p2.m_y;
        } else {
            return p1.m_y >= pt.m_y && pt.m_y >= p2.m_y;
        }
    }
}

template<typename NUM>
std::optional<Point<NUM>>
calculateArcCenters(const Point<NUM>& A, const Point<NUM>& B, NUM R, bool cclockwise)
{
    using ExtNum = typename NumTraits<NUM>::ExtendType;
    Point<NUM> M{(A.m_x + B.m_x) / 2, (A.m_y + B.m_y) / 2};
    const auto d = (B - A).EuclideanNorm();
    
    if (d > R * 2) {
        return std::nullopt;
    }
    
    const NUM h = NUM(std::sqrt(ExtNum(R)*R - (ExtNum(d) / 2.0) * (d / 2.0)));
    const auto p = (B-A).Perpendicular().Resize(h);
    
    const Point<NUM> C1{M.m_x + p.m_x, M.m_y + p.m_y};
    const Point<NUM> C2{M.m_x - p.m_x, M.m_y - p.m_y};
    return ((B - A).Cross(C1 - A) > 0) == cclockwise ? C1 : C2;
}

template<typename NUM>
struct ARC_SEGMENT_DATA: public SHAPE_TYPE_DATA<NUM> {
    Point<NUM> m_center, m_fromPt, m_toPt;
    NUM  m_radius;
    DAngleRange<NUM> m_range;

    ARC_SEGMENT_DATA(Point<NUM> a, Point<NUM> b, NUM radius, bool cclockwise):
        SHAPE_TYPE_DATA<NUM>(SHAPE_TYPE::ARC_SEGMENT),
        m_center(calculateArcCenters(a, b, radius, cclockwise).value()),
        m_fromPt(a), m_toPt(b), m_radius(radius),
        m_range(DAngle(a-m_center), DAngle(b-m_center), cclockwise) {}

    constexpr ARC_SEGMENT_DATA(Point<NUM> center, Point<NUM> from, Point<NUM> to, NUM radius, bool cclockwise):
        SHAPE_TYPE_DATA<NUM>(SHAPE_TYPE::ARC_SEGMENT),
        m_center(center), m_fromPt(from), m_toPt(to), m_radius(radius),
        m_range(DAngle<NUM>(from-m_center), DAngle<NUM>(to-m_center), cclockwise) {}

    DAngleRange<typename NumTraits<NUM>::ExtendType>
    ExtenedRange() const
    {
        return DAngleRange<typename NumTraits<NUM>::ExtendType>(
                m_range.m_from.ExtendedAngle(),
                m_range.m_to.ExtendedAngle(),
                m_range.m_cclockwise);
    }

    Box2D<NUM> box() const {
        Box2D<NUM> ans = Box2D(m_fromPt).merge(m_toPt);
        if (m_range.contains(DAngle<NUM>::Angle0())) {
            ans = ans.merge(m_center + Vector2<NUM>(m_radius, 0));
        }
        if (m_range.contains(DAngle<NUM>::Angle90())) {
            ans = ans.merge(m_center + Vector2<NUM>(0, m_radius));
        }
        if (m_range.contains(DAngle<NUM>::Angle180())) {
            ans = ans.merge(m_center + Vector2<NUM>(-m_radius, 0));
        }
        if (m_range.contains(DAngle<NUM>::Angle270())) {
            ans = ans.merge(m_center + Vector2<NUM>(0, -m_radius));
        }
        return ans;
    }

    Point<NUM> p1() const { return m_center + Point<NUM>{m_range.m_from.m_x, m_range.m_from.m_y}.Resize(m_radius); }
    Point<NUM> p2() const { return m_center + Point<NUM>{m_range.m_to.m_x, m_range.m_to.m_y}.Resize(m_radius); }

    constexpr bool operator==(const ARC_SEGMENT_DATA& oth) const {
        return m_center == oth.m_center && m_radius == oth.m_radius &&
               m_fromPt == oth.m_fromPt && m_toPt == oth.m_toPt && m_range == oth.m_range;
    }
    constexpr bool operator!=(const ARC_SEGMENT_DATA& oth) const { return !operator==(oth); }
};

template<typename NUM>
struct CIRCLE_DATA: public SHAPE_TYPE_DATA<NUM> {
    Point<NUM> m_center;
    NUM m_radius;

    constexpr CIRCLE_DATA(Point<NUM> center, NUM radius):
        SHAPE_TYPE_DATA<NUM>(SHAPE_TYPE::CIRCLE), m_center(center), m_radius(radius) {}

    constexpr Box2D<NUM> box() const {
        return Box2D<NUM>(m_center + Vector2<NUM>(m_radius, 0))
            .merge(m_center + Vector2<NUM>(0, m_radius))
            .merge(m_center + Vector2<NUM>(-m_radius, 0))
            .merge(m_center + Vector2<NUM>(0, -m_radius));
    }

    constexpr bool operator==(const CIRCLE_DATA& oth) const { return m_center == oth.m_center && m_radius == oth.m_radius; }
    constexpr bool operator!=(const CIRCLE_DATA& oth) const { return !operator==(oth); }
};

template<typename NUM>
struct POLYGON_DATA: public SHAPE_TYPE_DATA<NUM>
{
    qarray<Point<NUM>,4> m_points;

    template<typename Iter>
    POLYGON_DATA(Iter begin, Iter end):
        SHAPE_TYPE_DATA<NUM>(SHAPE_TYPE::POLYGON), m_points(begin, end) {};

    constexpr POLYGON_DATA():
        SHAPE_TYPE_DATA<NUM>(SHAPE_TYPE::POLYGON) {};

    constexpr Box2D<NUM> box() const {
        Box2D<NUM> ans;
        for (auto& pt: m_points) ans = ans.merge(pt);
        return ans;
    }

    constexpr bool operator==(const POLYGON_DATA& oth) const { return m_points == oth.m_points; }
    constexpr bool operator!=(const POLYGON_DATA& oth) const { return !operator==(oth); }
};

enum class POLYGON_NODE_TYPE {
    NORMAL, ARC,
};
template<typename NUM>
struct POLYGON_NORMAL_NODE
{
    POLYGON_NODE_TYPE m_type;
    Point<NUM> m_point;
};
template<typename NUM>
struct POLYGON_ARC_NODE: public POLYGON_NORMAL_NODE<NUM>
{
    NUM  m_radius;
    bool m_cclockwise;

    POLYGON_ARC_NODE(Point<NUM> pt, int radius, bool cclockwise):
        POLYGON_NORMAL_NODE<NUM>{POLYGON_NODE_TYPE::ARC, pt},
        m_radius(radius), m_cclockwise(cclockwise) { }
};
template<typename NUM>
union POLYGON_NODE {
    POLYGON_NORMAL_NODE<NUM> m_normal;
    POLYGON_ARC_NODE<NUM> m_arc;
    const auto& point() const { return m_normal.m_point; }

    explicit POLYGON_NODE(const POLYGON_NORMAL_NODE<NUM>& n): m_normal(n) {}
    explicit POLYGON_NODE(const POLYGON_ARC_NODE<NUM>& n): m_arc(n) {}

    POLYGON_NODE(POLYGON_NODE&& oth) {
        if (oth.m_normal.m_type == POLYGON_NODE_TYPE::NORMAL) {
            new (&m_normal) POLYGON_NORMAL_NODE<NUM>(std::move(oth.m_normal));
        } else if (oth.m_normal.m_type == POLYGON_NODE_TYPE::ARC) {
            new (&m_arc) POLYGON_ARC_NODE<NUM>(std::move(oth.m_arc));
        } else {
            H2GUnreachable();
        }
    }

    POLYGON_NODE(const POLYGON_NODE& oth) {
        if (oth.m_normal.m_type == POLYGON_NODE_TYPE::NORMAL) {
            new (&m_normal) POLYGON_NORMAL_NODE<NUM>(oth.m_normal);
        } else if (oth.m_normal.m_type == POLYGON_NODE_TYPE::ARC) {
            new (&m_arc) POLYGON_ARC_NODE<NUM>(oth.m_arc);
        } else {
            H2GUnreachable();
        }
    }

    auto type() const { return m_normal.m_type; }

    POLYGON_NODE& operator=(const POLYGON_NODE& oth) {
        ~POLYGON_NODE();
        new (this) POLYGON_NODE(oth);
    }

    POLYGON_NODE& operator=(POLYGON_NODE&& oth) {
        ~POLYGON_NODE();
        new (this) POLYGON_NODE(std::move(oth));
    }

    bool operator==(const POLYGON_NODE& oth) const {
        if (type() != oth.type()) {
            return false;
        }
        if (type() == POLYGON_NODE_TYPE::NORMAL) {
            return this->point() == oth.point();
        } else if (type() == POLYGON_NODE_TYPE::ARC) {
            return m_arc.m_point == oth.m_arc.m_point && m_arc.m_radius == oth.m_arc.m_radius &&
                   m_arc.m_cclockwise == oth.m_arc.m_cclockwise;
        }

        return false;
    }

    bool operator!=(const POLYGON_NODE& oth) const { return !operator==(oth); }

    ~POLYGON_NODE() = default;

    static POLYGON_NODE MakeNormal(Point<NUM> pt)
    {
        return POLYGON_NODE(POLYGON_NORMAL_NODE<NUM>{POLYGON_NODE_TYPE::NORMAL, pt});
    }
    static POLYGON_NODE MakeArc(Point<NUM> pt, int radius, bool cclockwise)
    {
        return POLYGON_NODE(POLYGON_ARC_NODE<NUM>(pt, radius, cclockwise));
    }
};
template<typename NUM>
struct COMPLEX_POLYGON_DATA: public SHAPE_TYPE_DATA<NUM>
{
    qarray<POLYGON_NODE<NUM>,4> m_points;
    template<typename Iter>
    COMPLEX_POLYGON_DATA(Iter begin, Iter end):
        SHAPE_TYPE_DATA<NUM>(SHAPE_TYPE::COMPLEX_POLYGON), m_points(begin, end) {};

    constexpr COMPLEX_POLYGON_DATA():
        SHAPE_TYPE_DATA<NUM>(SHAPE_TYPE::COMPLEX_POLYGON) {};

    constexpr Box2D<NUM> box() const {
        Box2D<NUM> ans;
        for (size_t i=0;i<m_points.size();i++) {
            const auto npt = m_points.at((i+1)%m_points.size()).m_normal.m_point;
            const auto& pt = m_points.at(i);
            if (pt.m_normal.m_type == POLYGON_NODE_TYPE::NORMAL) {
                ans = ans.merge(npt).merge(pt.m_normal.m_point);
            } else {
                ans = ans.merge(ARC_SEGMENT_DATA<NUM>(pt.m_arc.m_point, npt, pt.m_arc.m_radius, pt.m_arc.m_cclockwise).box());
            }
        }
        return ans;
    }

    constexpr bool operator==(const COMPLEX_POLYGON_DATA& oth) const { return m_points == oth.m_points; }
    constexpr bool operator!=(const COMPLEX_POLYGON_DATA& oth) const { return !operator==(oth); }
};

template<typename NUM>
union SHAPE_DATA {
    SHAPE_TYPE_DATA<NUM> m_type;
    ARC_SEGMENT_DATA<NUM> m_arc_seg;
    SEGMENT_DATA<NUM> m_seg;
    CIRCLE_DATA<NUM>  m_circle;
    POLYGON_DATA<NUM> m_polygon;
    COMPLEX_POLYGON_DATA<NUM> m_complexPolygon;

    SHAPE_TYPE type() const { return m_type.m_type; }

    struct SEGMENT_CTOR {};
    struct ARC_SEGMENT_CTOR {};
    struct CIRCLE_CTOR {};
    struct POLYGON_CTOR {};
    struct COMPLEX_POLYGON_CTOR {};

    SHAPE_DATA(SEGMENT_CTOR, Point<NUM> p1, Point<NUM> p2): m_seg(p1, p2) {}
    SHAPE_DATA(ARC_SEGMENT_CTOR, Point<NUM> p1, Point<NUM> p2, NUM radius, bool cclockwise): 
        m_arc_seg(p1, p2, radius, cclockwise) {}
    SHAPE_DATA(CIRCLE_CTOR, Point<NUM> center, NUM radius): 
        m_circle(center, radius) {}
    template<typename Iter>
    SHAPE_DATA(POLYGON_CTOR, Iter begin, Iter end): 
        m_polygon(begin, end) {}
    explicit SHAPE_DATA(POLYGON_CTOR): m_polygon() {}
    template<typename Iter>
    SHAPE_DATA(COMPLEX_POLYGON_CTOR, Iter begin, Iter end): 
        m_complexPolygon(begin, end) {}
    explicit SHAPE_DATA(COMPLEX_POLYGON_CTOR): m_complexPolygon() {}

    SHAPE_DATA(SHAPE_DATA&& other): m_type(SHAPE_TYPE::NONE) {
        switch (other.type()) {
        case SHAPE_TYPE::POLYGON:
            new (&m_polygon) POLYGON_DATA<NUM>(std::move(other.m_polygon));
            break;
        case SHAPE_TYPE::COMPLEX_POLYGON:
            new (&m_complexPolygon) COMPLEX_POLYGON_DATA<NUM>(std::move(other.m_complexPolygon));
            break;
        case SHAPE_TYPE::CIRCLE:
            new (&m_circle) CIRCLE_DATA<NUM>(std::move(other.m_circle));
            break;
        case SHAPE_TYPE::SEGMENT:
            new (&m_seg) SEGMENT_DATA<NUM>(std::move(other.m_seg));
            break;
        case SHAPE_TYPE::ARC_SEGMENT:
            new (&m_arc_seg) ARC_SEGMENT_DATA<NUM>(std::move(other.m_arc_seg));
            break;
        default:
            break;
        }
    }

    SHAPE_DATA(const SHAPE_DATA& other): m_type(SHAPE_TYPE::NONE) {
        switch (other.type()) {
        case SHAPE_TYPE::POLYGON:
            new (&m_polygon) POLYGON_DATA<NUM>(other.m_polygon);
            break;
        case SHAPE_TYPE::COMPLEX_POLYGON:
            new (&m_complexPolygon) COMPLEX_POLYGON_DATA<NUM>(other.m_complexPolygon);
            break;
        case SHAPE_TYPE::CIRCLE:
            new (&m_circle) CIRCLE_DATA<NUM>(other.m_circle);
            break;
        case SHAPE_TYPE::SEGMENT:
            new (&m_seg) SEGMENT_DATA<NUM>(other.m_seg);
            break;
        case SHAPE_TYPE::ARC_SEGMENT:
            new (&m_arc_seg) ARC_SEGMENT_DATA<NUM>(other.m_arc_seg);
            break;
        default:
            break;
        }
    }

    SHAPE_DATA& operator=(const SHAPE_DATA& oth) {
        this->~SHAPE_DATA();
        new (this) SHAPE_DATA(oth);
        return *this;
    }

    SHAPE_DATA& operator=(SHAPE_DATA&& oth) {
        this->~SHAPE_DATA();
        new (this) SHAPE_DATA(std::move(oth));
        return *this;
    }

    bool operator==(const SHAPE_DATA& oth) const {
        if (type() != oth.type()) {
            return false;
        }

        switch (type()) {
        case SHAPE_TYPE::POLYGON:
            return this->m_polygon == oth.m_polygon;
        case SHAPE_TYPE::COMPLEX_POLYGON:
            return this->m_complexPolygon == oth.m_complexPolygon;
        case SHAPE_TYPE::CIRCLE:
            return this->m_circle == oth.m_circle;
        case SHAPE_TYPE::ARC_SEGMENT:
            return this->m_arc_seg == oth.m_arc_seg;
        case SHAPE_TYPE::SEGMENT:
            return this->m_seg == oth.m_seg;
        case SHAPE_TYPE::NONE:
            return true;
        default:
            H2GUnreachable();
        }
        return false;
    }
    bool operator!=(const SHAPE_DATA& oth) const { return !operator==(oth); }

    constexpr Box2D<NUM> box() const {
        switch (type()) {
        case SHAPE_TYPE::POLYGON:
            return this->m_polygon.box();
        case SHAPE_TYPE::COMPLEX_POLYGON:
            return this->m_complexPolygon.box();
        case SHAPE_TYPE::CIRCLE:
            return this->m_circle.box();
        case SHAPE_TYPE::ARC_SEGMENT:
            return this->m_arc_seg.box();
        case SHAPE_TYPE::SEGMENT:
            return this->m_seg.box();
        case SHAPE_TYPE::NONE:
            return Box2D<NUM>();
        default:
            H2GUnreachable();
        }
        return Box2D<NUM>();
    }

    ~SHAPE_DATA() {
        switch (type()) {
        case SHAPE_TYPE::POLYGON:
            m_polygon.~POLYGON_DATA<NUM>();
            break;
        case SHAPE_TYPE::COMPLEX_POLYGON:
            m_complexPolygon.~COMPLEX_POLYGON_DATA<NUM>();
            break;
        default:
            break;
        }
        m_type.m_type = SHAPE_TYPE::NONE;
    }
};

template<typename NUM, bool CONST>
class SegmentOps {
public:
    using DataPtr = std::conditional_t<CONST,const SEGMENT_DATA<NUM>*,SEGMENT_DATA<NUM>*>;
    explicit SegmentOps( DataPtr data): m_dataPtr(data) {}

    template<typename = std::enable_if_t<CONST>>
    SegmentOps<NUM,false> AsMut() const
    {
        return SegmentOps<NUM,false>(const_cast<SEGMENT_DATA<NUM>*>(m_dataPtr));
    }

    auto& from() const { return m_dataPtr->m_a; }
    auto& to() const { return m_dataPtr->m_b; }

    auto box() const { return data().box(); }

    auto& data() const { return *m_dataPtr; }

private:
     DataPtr m_dataPtr;
};

template<typename NUM, bool CONST>
class ArcOps {
public:
    using DataPtr = std::conditional_t<CONST,const ARC_SEGMENT_DATA<NUM>*,ARC_SEGMENT_DATA<NUM>*>;
    explicit ArcOps( DataPtr data): m_dataPtr(data) {}

    template<typename = std::enable_if_t<CONST>>
    ArcOps<NUM,false> AsMut() const
    {
        return ArcOps<NUM,false>(const_cast<ARC_SEGMENT_DATA<NUM>*>(m_dataPtr));
    }
    
    auto& from() const { return data().m_fromPt; }
    auto& to()   const { return data().m_toPt; }
    auto& center() const { return data().m_center; }
    auto  radius() const { return data().m_radius; }

    auto box() const { return data().box(); }

    const ARC_SEGMENT_DATA<NUM>& data() const { return *m_dataPtr; }

private:
    DataPtr m_dataPtr;
};

template<typename NUM, bool CONST>
class CircleOps {
public:
    using DataPtr = std::conditional_t<CONST,const CIRCLE_DATA<NUM>*,CIRCLE_DATA<NUM>*>;
    explicit CircleOps( DataPtr data): m_dataPtr(data) {}

    template<typename = std::enable_if_t<CONST>>
    CircleOps<NUM,false> AsMut() const
    {
        return CircleOps<NUM,false>(const_cast<CIRCLE_DATA<NUM>*>(m_dataPtr));
    }

    auto& center() const { return data().m_center; }
    auto  radius() const { return data().m_radius; }

    auto box() const { return data().box(); }

    auto& data() const { return *m_dataPtr; }

private:
    DataPtr m_dataPtr;
};

template<typename NUM, bool CONST>
class PolygonOps {
public:
    using DataPtr = std::conditional_t<CONST,const POLYGON_DATA<NUM>*,POLYGON_DATA<NUM>*>;
    explicit PolygonOps( DataPtr data): m_dataPtr(data) {}

    template<typename = std::enable_if_t<CONST>>
    PolygonOps<NUM,false> AsMut() const
    {
        return PolygonOps<NUM,false>(const_cast<POLYGON_DATA<NUM>*>(m_dataPtr));
    }

    size_t size() const { return data().m_points.size(); }

    const SEGMENT_DATA<NUM>& GetSeg(size_t i) const
    {
        H2GAssert(i <= size());
        const size_t next = (i + 1) % size();
        return SEGMENT_DATA<NUM>(data().m_points.at(i), data().m_points.at(next));
    }

    const Point<NUM>& GetPoint(size_t i) const
    {
        H2GAssert(i <= size());
        return data().m_points.at(i);
    }

    template<typename = std::enable_if_t<CONST>>
    void PushPoint(Point<NUM>& pt)
    {
        pdata().m_points.push_back(pt);
    }

    auto box() const { return data().box(); }

    const POLYGON_DATA<NUM>& data() const { return *m_dataPtr; }

private:
    template<typename = std::enable_if_t<CONST>>
    POLYGON_DATA<NUM>& pdata() { return *m_dataPtr; }

    DataPtr m_dataPtr;
};

template<typename NUM, bool CONST>
class ComplexPolygonOps {
public:
    using DataPtr = std::conditional_t<CONST,const COMPLEX_POLYGON_DATA<NUM>*,COMPLEX_POLYGON_DATA<NUM>*>;
    explicit ComplexPolygonOps( DataPtr data): m_dataPtr(data) {}

    template<typename = std::enable_if_t<CONST>>
    ComplexPolygonOps<NUM,false> AsMut() const
    {
        return ComplexPolygonOps<NUM,false>(const_cast<COMPLEX_POLYGON_DATA<NUM>*>(m_dataPtr));
    }

    using GeneralSegment = std::variant<SEGMENT_DATA<NUM>,ARC_SEGMENT_DATA<NUM>>;
    GeneralSegment GetSeg(size_t i) const
    {
        const auto n = (i + 1) % size();
        const POLYGON_NODE<NUM>& pt = data().m_points.at(i);
        const auto npt = data().m_points.at(n).m_normal.m_point;
        if (pt.type() == POLYGON_NODE_TYPE::NORMAL) {
            return SEGMENT_DATA<NUM>(pt.m_normal.m_point, npt);
        } else if (pt.type() == POLYGON_NODE_TYPE::ARC) {
            return ARC_SEGMENT_DATA<NUM>(pt.m_arc.m_point, npt, pt.m_arc.m_radius, pt.m_arc.m_cclockwise);
        } else {
            H2GUnreachable();
        }
    }

    size_t size() const { return data().m_points.size(); }

    auto box() const { return data().box(); }

    const COMPLEX_POLYGON_DATA<NUM>& data() const { return *m_dataPtr; }

private:
    template<typename = std::enable_if_t<CONST>>
    COMPLEX_POLYGON_DATA<NUM>& pdata() { return *m_dataPtr; }

    DataPtr m_dataPtr;
};

template<typename NUM>
class SHAPE {
    using UData = SHAPE_DATA<NUM>;

public:
    using PointType = Point<NUM>;

    SHAPE() = delete;
    static SHAPE createLineSegment(PointType p1, PointType p2)
    {
        return SHAPE(UData(typename UData::SEGMENT_CTOR(), p1, p2));
    }
    static SHAPE createArcSegment(PointType p1, PointType p2, NUM radius, bool cclockwise)
    {
        return SHAPE(UData(typename UData::ARC_SEGMENT_CTOR(), p1, p2, radius, cclockwise));
    }
    static SHAPE createCircle(PointType center, NUM radius)
    {
        return SHAPE(UData(typename UData::CIRCLE_CTOR(), center, radius));
    }
    static SHAPE createPolygon()
    {
        return SHAPE(UData(typename UData::POLYGON_CTOR()));
    }
    template<typename Container>
    static SHAPE createPolygon(const Container& container)
    {
        return SHAPE(UData(typename UData::POLYGON_CTOR(), container.begin(), container.end()));
    }
    template<typename Iter>
    static SHAPE createPolygon(Iter begin, Iter end)
    {
        return SHAPE(UData(typename UData::POLYGON_CTOR(), begin, end));
    }
    static SHAPE createComplexPolygon()
    {
        return SHAPE(UData(typename UData::COMPLEX_POLYGON_CTOR()));
    }
    template<typename Iter>
    static SHAPE createComplexPolygon(Iter begin, Iter end)
    {
        return SHAPE(UData(typename UData::COMPLEX_POLYGON_CTOR(), begin, end));
    }
    template<typename Container>
    static SHAPE createComplexPolygon(const Container& container)
    {
        return SHAPE(UData(typename UData::COMPLEX_POLYGON_CTOR(), container.begin(), container.end()));
    }

    SegmentOps<NUM,true>        asSegment() const { return SegmentOps<NUM,true>(&m_data.m_seg); }
    ArcOps<NUM,true>            asArc()  const { return ArcOps<NUM,true>(&m_data.m_arc_seg); }
    CircleOps<NUM,true>         asCircle() const { return CircleOps<NUM,true>(&m_data.m_circle); }
    PolygonOps<NUM,true>        asPolygon() const { return PolygonOps<NUM,true>(&m_data.m_polygon); }
    ComplexPolygonOps<NUM,true> asComplexPolygon() const { return ComplexPolygonOps<NUM,true>(&m_data.m_complexPolygon); }

    SegmentOps<NUM,false>        asSegment() { return SegmentOps<NUM,false>(&m_data.m_seg); }
    ArcOps<NUM,false>            asArc()  { return ArcOps<NUM,false>(&m_data.m_arc_seg); }
    CircleOps<NUM,false>         asCircle() { return CircleOps<NUM,false>(&m_data.m_circle); }
    PolygonOps<NUM,false>        asPolygon() { return PolygonOps<NUM,false>(&m_data.m_polygon); }
    ComplexPolygonOps<NUM,false> asComplexPolygon() { return ComplexPolygonOps<NUM,false>(&m_data.m_complexPolygon); }

    SHAPE_TYPE type() const { return m_data.type(); }

    NUM distance(const SHAPE& oth, std::pair<Point<NUM>,Point<NUM>>&) const;

    Box2D<NUM> box() const { return m_data.box(); }

    bool operator==(const SHAPE& oth) const { return m_data == oth.m_data; }
    bool operator!=(const SHAPE& oth) const { return !operator==(oth); }

private:
    explicit SHAPE(SHAPE_DATA<NUM>&& data): m_data(data) {}
    UData m_data;
};

}



#include <type_traits>

namespace H2G {

struct SC_TAG_BASE {};
struct SC_TAG_DEFAULT: SC_TAG_BASE {};

template<typename NUM, typename SA, typename SB, typename TAG, typename = void>
struct SHAPE_DISTANCE_TRAIT {
    // static NUM distance_impl(const SA& shapea, const SB& shapeb, NearestPts& nearestPts) const;
};

template<typename NUM>
using _NearestPts = std::pair<Point<NUM>, Point<NUM>>;

template<typename NUM, typename SA, typename SB, typename TAG, typename = void>
struct distance_implemented: std::false_type {};
template<typename NUM, typename SA, typename SB, typename TAG>
struct distance_implemented<NUM, SA, SB, TAG,
    std::void_t<
    decltype(SHAPE_DISTANCE_TRAIT<NUM,SA,SB,TAG>::distance_impl(
                std::declval<const SA&>(),
                std::declval<const SB&>(),
                std::declval<_NearestPts<NUM>&>()))>>: std::true_type {};

template<typename NUM, typename SA, typename SB, typename TAG>
constexpr bool distance_implemented_v = distance_implemented<NUM, SA, SB, TAG>::value;

template<typename NUM, typename SA, typename SB, typename TAG = SC_TAG_DEFAULT>
NUM distance_dispatcher(const SA& a, const SB& b, _NearestPts<NUM>& pts)
{
    static_assert(std::is_base_of_v<SC_TAG_BASE, TAG>, "invalid tag");
    constexpr auto v1 = distance_implemented_v<NUM,SA,SB,TAG>;
    constexpr auto v2 = distance_implemented_v<NUM,SB,SA,TAG>;
    if constexpr (v1) {
        return SHAPE_DISTANCE_TRAIT<NUM,SA,SB,TAG>::distance_impl(a, b, pts);
    } else if constexpr (v2) {
        const auto ans = SHAPE_DISTANCE_TRAIT<NUM,SB,SA,TAG>::distance_impl(b, a, pts);
        std::swap(pts.first, pts.second);
        return ans;
    } else {
        static_assert(v1 || v2, "unimplemented distance trait");
        return NUM();
    }
}

template<typename NUM>
bool closerThan(const _NearestPts<NUM>& pts1, const _NearestPts<NUM>& pts2)
{
    const auto s1 = (pts1.first - pts1.second).SquaredEuclideanNorm();
    const auto s2 = (pts2.first - pts2.second).SquaredEuclideanNorm();
    return s1 < s2;
}

template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,Point<NUM>,Point<NUM>,SC_TAG_DEFAULT> {
    static NUM distance_impl(const Point<NUM>& p1, const Point<NUM>& p2, _NearestPts<NUM>& nearestPts)
    {
        nearestPts = {p1, p2};
        return (p2 - p1).EuclideanNorm();
    }
};

template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,SEGMENT_DATA<NUM>,Point<NUM>,SC_TAG_DEFAULT> {
    static NUM distance_impl(const SEGMENT_DATA<NUM>& seg, const Point<NUM>& pt, _NearestPts<NUM>& nearestPts)
    {
        const auto pn = nearestPointSegment(seg.m_a, seg.m_b, pt);
        nearestPts = {pn, pt};
        return (pn - pt).EuclideanNorm();
    }
};

template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,SEGMENT_DATA<NUM>,SEGMENT_DATA<NUM>,SC_TAG_DEFAULT> {
    static NUM distance_impl(const SEGMENT_DATA<NUM>& sega, const SEGMENT_DATA<NUM>& segb, _NearestPts<NUM>& nearestPts)
    {
        const auto ptOpt = lineSegmentsIntersect(sega.m_a, sega.m_b, segb.m_a, segb.m_b);
        if (ptOpt.has_value()) {
            nearestPts = {ptOpt.value(), ptOpt.value()};
            return NUM(0);
        }

        auto ans = distance_dispatcher(sega, segb.m_a, nearestPts);
        {
            _NearestPts<NUM> pts{Point<NUM>(0,0), Point<NUM>(0,0)};
            const auto v = distance_dispatcher(sega, segb.m_b, pts);
            if (closerThan(pts, nearestPts)) {
                ans = v;
                nearestPts = pts;
            }
        }
        {
            _NearestPts<NUM> pts{Point<NUM>(0,0), Point<NUM>(0,0)};
            const auto v = distance_dispatcher(sega.m_a, segb, pts);
            if (closerThan(pts, nearestPts)) {
                ans = v;
                nearestPts = pts;
            }
        }
        {
            _NearestPts<NUM> pts{Point<NUM>(0,0), Point<NUM>(0,0)};
            const auto v = distance_dispatcher(sega.m_b, segb, pts);
            if (closerThan(pts, nearestPts)) {
                ans = v;
                nearestPts = pts;
            }
        }
        return ans;
    }
};

template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,CIRCLE_DATA<NUM>,Point<NUM>,SC_TAG_DEFAULT> {
    static NUM distance_impl(const CIRCLE_DATA<NUM>& circle, const Point<NUM>& pt, _NearestPts<NUM>& nearestPts)
    {
        if ((circle.m_center - pt).EuclideanNorm() <= circle.m_radius) {
            nearestPts = {pt, pt};
            return NUM(0);
        }

        nearestPts = {circle.m_center + (pt - circle.m_center).Resize(circle.m_radius), pt};
        return (pt-circle.m_center).EuclideanNorm() - circle.m_radius;
    }
};

struct SC_TAG_EMPTY_CIRCLE: SC_TAG_BASE {};
template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,CIRCLE_DATA<NUM>,Point<NUM>,SC_TAG_EMPTY_CIRCLE> {
    static NUM distance_impl(const CIRCLE_DATA<NUM>& circle, const Point<NUM>& pt, _NearestPts<NUM>& nearestPts)
    {
        if (circle.m_center == pt) {
            nearestPts = {pt + Point<NUM>(circle.m_radius, 0), pt};
            return circle.m_radius;
        }
        nearestPts = {circle.m_center + (pt - circle.m_center).Resize(circle.m_radius), pt};
        return (nearestPts.first - nearestPts.second).EuclideanNorm();
    }
};

template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,CIRCLE_DATA<NUM>,CIRCLE_DATA<NUM>,SC_TAG_DEFAULT> {
    static NUM distance_impl(const CIRCLE_DATA<NUM>& circlea, const CIRCLE_DATA<NUM>& circleb, _NearestPts<NUM>& nearestPts)
    {
        if (circlea.m_center == circleb.m_center) {
            nearestPts = {circlea.m_center, circlea.m_center};
            return NUM(0);
        }
        const auto v = (circlea.m_center - circleb.m_center).SquaredEuclideanNorm();
        const auto r = static_cast<decltype(v)>(circleb.m_radius) + circleb.m_radius;
        const auto p1 = circlea.m_center + (circleb.m_center - circlea.m_center).Resize(circlea.m_radius);
        const auto p2 = circleb.m_center + (circlea.m_center - circleb.m_center).Resize(circleb.m_radius);
        if (r * r > v) {
            if (circlea.m_radius < circleb.m_radius) {
                nearestPts = {p1, p1};
            } else {
                nearestPts = {p2, p2};
            }
            return NUM(0);
        }

        nearestPts = {p1, p2};
        return (circlea.m_center - circleb.m_center).EuclideanNorm() - (circlea.m_radius + circleb.m_radius);
    }
};

template<typename NUM>
std::pair<Point<NUM>,std::optional<std::pair<Point<NUM>,Point<NUM>>>>
circle_line_intersection(const CIRCLE_DATA<NUM>& circle, const Point<NUM>& p1, const Point<NUM>& p2)
{
    using OPT = std::optional<std::pair<Point<NUM>,Point<NUM>>>;
    if (p1 == p2) {
        return {p1, OPT()};
    }

    using FltType = typename NumTraits<NUM>::FloatType;
    const auto pa = p1 - circle.m_center;
    const auto pb = p2 - circle.m_center;
    const auto v21 = p2 - p1;
    const auto vc1 = circle.m_center - p1;
    const auto d1 = v21.Dot(v21);
    const auto d2 = v21.Dot(vc1);
    const FltType lambda = FltType(d2) / d1;
    const auto sp = pa + Point<NUM>{NUM(v21.m_x * lambda), NUM(v21.m_y * lambda)};
    if (sp.EuclideanNorm() > circle.m_radius) {
        return {sp + circle.m_center, OPT()};
    }

    const auto sr = sp.SquaredEuclideanNorm();
    const NUM len = std::sqrt(decltype(sr)(circle.m_radius) * circle.m_radius - sr);
    const auto vec = v21.Resize(len);
    return {sp +circle.m_center, OPT(std::make_pair(sp + circle.m_center + vec, sp + circle.m_center - vec))};
}

template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,CIRCLE_DATA<NUM>,SEGMENT_DATA<NUM>,SC_TAG_DEFAULT> {
    static NUM distance_impl(const CIRCLE_DATA<NUM>& circle, const SEGMENT_DATA<NUM>& seg, _NearestPts<NUM>& nearestPts)
    {
        const auto& [sp, p1p2] = circle_line_intersection(circle, seg.m_a, seg.m_b);
        if (p1p2.has_value()) {
            if (pointInLine(seg.m_a, seg.m_b, p1p2.value().first)) {
                nearestPts = {p1p2.value().first, p1p2.value().first};
                return NUM(0);
            }
            if (pointInLine(seg.m_a, seg.m_b, p1p2.value().second)) {
                nearestPts = {p1p2.value().second, p1p2.value().second};
                return NUM(0);
            }
        }

        if ((circle.m_center - seg.m_a).EuclideanNorm() <= circle.m_radius) {
            nearestPts = {seg.m_a, seg.m_a};
            return NUM(0);
        }
        
        if (pointInLine(seg.m_a, seg.m_b, sp)) {
            nearestPts = { circle.m_center + (sp - circle.m_center).Resize(circle.m_radius), sp };
            return (nearestPts.second - nearestPts.first).EuclideanNorm();
        }

        const auto d1 = distance_dispatcher(circle, seg.m_a, nearestPts);
        auto pts = nearestPts;
        const auto d2 = distance_dispatcher(circle, seg.m_b, pts);
        if (closerThan(pts, nearestPts)) {
            nearestPts = pts;
            return d2;
        }
        return d1;
    }
};

template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,ARC_SEGMENT_DATA<NUM>,Point<NUM>,SC_TAG_DEFAULT> {
    static NUM distance_impl(const ARC_SEGMENT_DATA<NUM>& arc, const Point<NUM>& pt, _NearestPts<NUM>& nearestPts)
    {
        const CIRCLE_DATA<NUM> circle(arc.m_center, arc.m_radius);
        const auto num = distance_dispatcher<NUM, CIRCLE_DATA<NUM>, Point<NUM>, SC_TAG_EMPTY_CIRCLE>(circle, pt, nearestPts);
        if (arc.m_range.contains(DAngle(nearestPts.first - arc.m_center))) {
            return num;
        }

        if (closerThan(_NearestPts<NUM>{arc.p1(), pt}, _NearestPts<NUM>{arc.p2(), pt})) {
            nearestPts = {arc.p1(), pt};
            return (arc.p1() - pt).EuclideanNorm();
        } else {
            nearestPts = {arc.p2(), pt};
            return (arc.p2() - pt).EuclideanNorm();
        }
    }
};

template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,ARC_SEGMENT_DATA<NUM>,SEGMENT_DATA<NUM>,SC_TAG_DEFAULT> {
    static NUM distance_impl(const ARC_SEGMENT_DATA<NUM>& arc, const SEGMENT_DATA<NUM>& seg, _NearestPts<NUM>& nearestPts)
    {
        const CIRCLE_DATA<NUM> circle(arc.m_center, arc.m_radius);
        const auto& [sp, p1p2] = circle_line_intersection(circle, seg.m_a, seg.m_b);
        if (p1p2.has_value()) {
            if (pointInLine(seg.m_a, seg.m_b, p1p2.value().first) &&
                arc.m_range.contains(DAngle<NUM>(p1p2.value().first - arc.m_center)))
            {
                nearestPts = {p1p2.value().first, p1p2.value().first};
                return NUM(0);
            }
            if (pointInLine(seg.m_a, seg.m_b, p1p2.value().second) &&
                arc.m_range.contains(DAngle<NUM>(p1p2.value().second - arc.m_center)))
            {
                nearestPts = {p1p2.value().second, p1p2.value().second};
                return NUM(0);
            }
        }

        if (pointInLine(seg.m_a, seg.m_b, sp) &&
            arc.m_range.contains(DAngle<NUM>(sp - arc.m_center)))
        {
            nearestPts = { circle.m_center + (sp - circle.m_center).Resize(circle.m_radius), sp };
            return (nearestPts.second - nearestPts.first).EuclideanNorm();
        }

        auto d1 = distance_dispatcher(arc, seg.m_a, nearestPts);
        {
            auto pts = nearestPts;
            const auto d = distance_dispatcher(arc, seg.m_b, pts);
            if (closerThan(pts, nearestPts)) {
                d1 = d;
                nearestPts = pts;
            }
        }
        {
            auto pts = nearestPts;
            const auto d = distance_dispatcher(arc.p1(), seg, pts);
            if (closerThan(pts, nearestPts)) {
                d1 = d;
                nearestPts = pts;
            }
        }
        {
            auto pts = nearestPts;
            const auto d = distance_dispatcher(arc.p2(), seg, pts);
            if (closerThan(pts, nearestPts)) {
                d1 = d;
                nearestPts = pts;
            }
        }
        return d1;
    }
};

template<typename NUM>
struct CIRCLE_RELATION_DATA {
    enum class Cond { AInB, BInA, Intersected, Away };
    Cond m_cond;
    std::optional<Point<NUM>> m_p1, m_p2, m_pi1, m_pi2;
};
template<typename NUM>
CIRCLE_RELATION_DATA<NUM> circle_relation(const CIRCLE_DATA<NUM>& c1, const CIRCLE_DATA<NUM>& c2)
{
    CIRCLE_RELATION_DATA<NUM> ans{CIRCLE_RELATION_DATA<NUM>::Cond::Away};
    if (c1.m_center == c2.m_center) {
        ans.m_p1 = c1.m_center + Point<NUM>(c1.m_radius, 0);
        ans.m_p2 = c2.m_center + Point<NUM>(c2.m_radius, 0);
        if (c1.m_radius < c2.m_radius) {
            ans.m_cond = CIRCLE_RELATION_DATA<NUM>::Cond::AInB;
        } else {
            ans.m_cond = CIRCLE_RELATION_DATA<NUM>::Cond::BInA;
        }
        return ans;
    }

    const auto center2 = c2.m_center - c1.m_center;
    const auto sn = center2.SquaredEuclideanNorm();
    const auto rsum = c1.m_radius + c2.m_radius;
    const auto rsumSquared = decltype(sn)(rsum) * rsum;
    if (sn > rsumSquared) {
        ans.m_p1 = center2.Resize(c1.m_radius) + c1.m_center;
        ans.m_p2 = center2.Resize(-c2.m_radius) + c1.m_center;
        return ans;
    }

    const auto rdiff = c1.m_radius - c2.m_radius;
    const auto rdiffSquared = decltype(sn)(rdiff) * rdiff;
    if (rdiffSquared >= sn) {
        if (c1.m_radius < c2.m_radius) {
            ans.m_cond = CIRCLE_RELATION_DATA<NUM>::Cond::AInB;
            ans.m_p1 = center2.Resize(-c1.m_radius) + c1.m_center;
            ans.m_p2 = center2 + center2.Resize(-c2.m_radius) + c1.m_center;
        } else {
            ans.m_cond = CIRCLE_RELATION_DATA<NUM>::Cond::BInA;
            ans.m_p1 = center2.Resize(c1.m_radius) + c1.m_center;
            ans.m_p2 = center2 + center2.Resize(c2.m_radius) + c1.m_center;
        }
        return ans;
    }

    // FIXME numerical stability and numeric_limits
    ans.m_cond = CIRCLE_RELATION_DATA<NUM>::Cond::Intersected;
    using ExtNUM = typename NumTraits<NUM>::ExtendType;
    const auto R2 = center2.SquaredEuclideanNorm();
    const auto A = ExtNUM(c1.m_radius) * c1.m_radius - ExtNUM(c2.m_radius) * c2.m_radius + R2;
    const ExtNUM M = std::sqrt(4 * ExtNUM(c1.m_radius) * c1.m_radius * R2 - A * A);
    const auto valfunc = [&](NUM k1, NUM k2) -> NUM { return (A * k1 + M * k2) / (2 * R2); };
    ans.m_pi1 = Point<NUM>(valfunc(center2.m_x,  center2.m_y), valfunc(center2.m_y, -center2.m_x)) + c1.m_center;
    ans.m_pi2 = Point<NUM>(valfunc(center2.m_x, -center2.m_y), valfunc(center2.m_y,  center2.m_x)) + c1.m_center;
    return ans;
}

template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,ARC_SEGMENT_DATA<NUM>,ARC_SEGMENT_DATA<NUM>,SC_TAG_DEFAULT> {
    static NUM distance_impl(const ARC_SEGMENT_DATA<NUM>& arc1, const ARC_SEGMENT_DATA<NUM>& arc2, _NearestPts<NUM>& nearestPts)
    {
        const CIRCLE_DATA<NUM> circle1(arc1.m_center, arc1.m_radius);
        const CIRCLE_DATA<NUM> circle2(arc2.m_center, arc2.m_radius);
        const auto relation = circle_relation(circle1, circle2);
        switch (relation.m_cond) {
        case CIRCLE_RELATION_DATA<NUM>::Cond::AInB:
        case CIRCLE_RELATION_DATA<NUM>::Cond::BInA:
        case CIRCLE_RELATION_DATA<NUM>::Cond::Away:
            if (arc1.m_range.contains(DAngle<NUM>(relation.m_p1.value() - arc1.m_center)) &&
                arc2.m_range.contains(DAngle<NUM>(relation.m_p2.value() - arc2.m_center)))
            {
                nearestPts = {relation.m_p1.value(), relation.m_p2.value()};
                return (relation.m_p1.value() - relation.m_p2.value()).EuclideanNorm();
            }
            break;
        case CIRCLE_RELATION_DATA<NUM>::Cond::Intersected:
            if (arc1.m_range.contains(DAngle<NUM>(relation.m_pi1.value() - arc1.m_center)) &&
                arc2.m_range.contains(DAngle<NUM>(relation.m_pi1.value() - arc2.m_center)))
            {
                nearestPts = {relation.m_pi1.value(), relation.m_pi1.value()};
                return NUM(0);
            }
            if (arc1.m_range.contains(DAngle<NUM>(relation.m_pi2.value() - arc1.m_center)) &&
                arc2.m_range.contains(DAngle<NUM>(relation.m_pi2.value() - arc2.m_center)))
            {
                nearestPts = {relation.m_pi2.value(), relation.m_pi2.value()};
                return NUM(0);
            }
            break;
        default:
            H2GUnreachable();
        }

        auto dis = distance_dispatcher(arc1.p1(), arc2, nearestPts);
        {
            auto pts = nearestPts;
            const auto d = distance_dispatcher(arc1.p2(), arc2, pts);
            if (closerThan(pts, nearestPts)) {
                dis = d;
                nearestPts = pts;
            }
        }
        {
            auto pts = nearestPts;
            const auto d = distance_dispatcher(arc1, arc2.p1(), pts);
            if (closerThan(pts, nearestPts)) {
                dis = d;
                nearestPts = pts;
            }
        }
        {
            auto pts = nearestPts;
            const auto d = distance_dispatcher(arc1, arc2.p2(), pts);
            if (closerThan(pts, nearestPts)) {
                dis = d;
                nearestPts = pts;
            }
        }
        return dis;
    }
};

template<typename NUM>
struct SHAPE_DISTANCE_TRAIT<NUM,ARC_SEGMENT_DATA<NUM>,CIRCLE_DATA<NUM>,SC_TAG_DEFAULT> {
    static NUM distance_impl(const ARC_SEGMENT_DATA<NUM>& arc, const CIRCLE_DATA<NUM>& circle, _NearestPts<NUM>& nearestPts)
    {
        const CIRCLE_DATA<NUM> arc_circle(arc.m_center, arc.m_radius);
        const auto relation = circle_relation(arc_circle, circle);
        switch (relation.m_cond) {
        case CIRCLE_RELATION_DATA<NUM>::Cond::AInB:
            nearestPts = {arc.p1(), arc.p1()};
            return NUM(0);
        case CIRCLE_RELATION_DATA<NUM>::Cond::BInA:
        case CIRCLE_RELATION_DATA<NUM>::Cond::Away:
            if (arc.m_range.contains(DAngle<NUM>(relation.m_p1.value() - arc.m_center)))
            {
                nearestPts = {relation.m_p1.value(), relation.m_p2.value()};
                return (relation.m_p1.value() - relation.m_p2.value()).EuclideanNorm();
            }
            break;
        case CIRCLE_RELATION_DATA<NUM>::Cond::Intersected:
            if (arc.m_range.contains(DAngle<NUM>(relation.m_pi1.value() - arc.m_center)))
            {
                nearestPts = {relation.m_pi1.value(), relation.m_pi1.value()};
                return NUM(0);
            }
            if (arc.m_range.contains(DAngle<NUM>(relation.m_pi2.value() - arc.m_center)))
            {
                nearestPts = {relation.m_pi2.value(), relation.m_pi2.value()};
                return NUM(0);
            }
            break;
        default:
            H2GUnreachable();
        }

        auto dis = distance_dispatcher(arc.p1(), circle, nearestPts);
        {
            auto pts = nearestPts;
            const auto d = distance_dispatcher(arc.p2(), circle, pts);
            if (closerThan(pts, nearestPts)) {
                dis = d;
                nearestPts = pts;
            }
        }
        return dis;
    }
};

template<typename NUM>
size_t rayCastToLineSegmentAbove(const Point<NUM>& pt, const SEGMENT_DATA<NUM>& seg)
{
    if (seg.m_a.m_y == seg.m_b.m_y) return 0;
    const auto alt = seg.m_a.m_y < seg.m_b.m_y;
    const auto ptmax = alt ? seg.m_b : seg.m_a;
    const auto ptmin = alt ? seg.m_a : seg.m_b;
    if (pt.m_y >= ptmax.m_y || pt.m_y < ptmin.m_y) return 0;

    return (ptmax - ptmin).Cross(pt - ptmin) > 0 ? 1 : 0;
}

template<typename NUM>
size_t rayCastToArcSegmentAbove(const Point<NUM>& pt, const ARC_SEGMENT_DATA<NUM>& arc)
{
    if (pt.m_y - arc.m_center.m_y >= arc.m_radius ||
        arc.m_center.m_y - pt.m_y > arc.m_radius)
    {
        return 0;
    }

    if (pt.m_y + arc.m_radius == arc.m_center.m_y) {
        if (arc.m_range.contains(DAngle<NUM>::Angle270())) {
            if (arc.m_range.m_from == DAngle<NUM>::Angle270() ||
                arc.m_range.m_to == DAngle<NUM>::Angle270())
            {
                return 1;
            } else {
                return 2;
            }
        } else {
            return 0;
        }
    }

    using ExtNum = typename NumTraits<NUM>::ExtendType;
    const ExtNum diffy = pt.m_y - arc.m_center.m_y;
    const auto adiffy2 = static_cast<ExtNum>(diffy) * diffy;
    const auto r2 = ExtNum(arc.m_radius) * arc.m_radius;
    const auto diffx2 = r2 - adiffy2;
    const auto diffy2 = diffy > 0 ? adiffy2 : -adiffy2;
    const auto dn = pt.m_x - arc.m_center.m_x;
    size_t ans = 0;
    const auto extRange = arc.ExtenedRange();
    if (arc.m_center.m_x > pt.m_x || diffx2 > static_cast<ExtNum>(dn) * dn) {
        const DAngle<ExtNum> angle{diffx2, diffy2};
        if (extRange.contains(angle)) {
            if (extRange.m_from == angle) {
                if (extRange.m_cclockwise) {
                    ans++;
                } else {
                    if (extRange.m_to == angle) {
                        ans++;
                    }
                }
            } else if (extRange.m_to == angle) {
                if (extRange.m_cclockwise) {
                    if (extRange.m_from == angle) {
                        ans++;
                    }
                } else {
                    ans++;
                }
            } else {
                ans++;
            }
        }
    }
    if (arc.m_center.m_x > pt.m_x && static_cast<ExtNum>(dn) * dn > diffx2) {
        const DAngle<ExtNum> angle{-diffx2, diffy2};
        if (extRange.contains(angle)) {
            if (extRange.m_from == angle) {
                if (extRange.m_cclockwise) {
                    if (extRange.m_to == angle) {
                        ans++;
                    }
                } else {
                    ans++;
                }
            } else if (extRange.m_to == angle) {
                if (extRange.m_cclockwise) {
                    ans++;
                } else {
                    if (extRange.m_from == angle) {
                        ans++;
                    }
                }
            } else {
                ans++;
            }
        }

    }

    return ans;
}

template<typename NUM>
inline bool circle_pt_compare_less(NUM v1, NUM xc, NUM r, NUM xd, NUM yd)
{
    H2GAssert(r > 0);
    using ExtNum = typename NumTraits<NUM>::ExtendType;
    const ExtNum a = static_cast<ExtNum>(v1 - xc) * (v1 - xc);
    const ExtNum b = static_cast<ExtNum>(r) * (r);
    const ExtNum c = static_cast<ExtNum>(xd) * xd;
    const ExtNum d = static_cast<ExtNum>(yd) * yd + c;
    const int s1 = v1 - xc > 0 ? 1 : (v1 == xc ? 0 : -1);
    const int s2 = xd > 0 ? 1 : (xd == 0 ? 0 : -1);
    if (s1 != s2) {
        return s1 < s2;
    }
    if (s1 < 0) {
        return compare_aob2cod(c, d, a, b);
    } else if (s1 == 0) {
        return false;
    } else {
        return compare_aob2cod(a, b, c, d);
    }
}

template<typename NUM>
inline bool circle_pt_compare_greater(NUM v1, NUM xc, NUM r, NUM xd, NUM yd)
{
    H2GAssert(r > 0);
    using ExtNum = typename NumTraits<NUM>::ExtendType;
    const ExtNum a = static_cast<ExtNum>(v1 - xc) * (v1 - xc);
    const ExtNum b = static_cast<ExtNum>(r) * (r);
    const ExtNum c = static_cast<ExtNum>(xd) * xd;
    const ExtNum d = static_cast<ExtNum>(yd) * yd + c;
    const int s1 = v1 - xc > 0 ? 1 : (v1 == xc ? 0 : -1);
    const int s2 = xd > 0 ? 1 : (xd == 0 ? 0 : -1);
    if (s1 != s2) {
        return s1 > s2;
    }
    if (s1 < 0) {
        return compare_aob2cod(a, b, c, d);
    } else if (s1 == 0) {
        return false;
    } else {
        return compare_aob2cod(c, d, a, b);
    }
}

template<typename NUM>
inline size_t rayCastToLineSegmentArcPointAbove(const Point<NUM>& pt, const Point<NUM>& linep1,
        NUM radius, const Point<NUM>& center, const DAngle<NUM>& angle)
{
    using ExtNum = typename NumTraits<NUM>::ExtendType;
    const auto inLeft = [&]() -> std::optional<bool> {
        if (circle_pt_compare_less(pt.m_x, center.m_x, radius, angle.m_x, angle.m_y) && pt.m_x < linep1.m_x) {
            return true;
        } else if (circle_pt_compare_greater(pt.m_x, center.m_x, radius, angle.m_x, angle.m_y) && pt.m_x > linep1.m_x) {
            return false;
        } else {
            return std::nullopt;
        }
    };
    const auto ptx = pt - linep1;
    const auto centerx = center - linep1;
    const auto vala = centerx.m_x * ptx.m_y - ptx.m_x * centerx.m_y;
    const auto valb = ptx.m_x * angle.m_y - angle.m_x * ptx.m_y;;
    const auto s1 = vala > 0 ? 1 : (vala == 0 ? 0 : -1);
    const auto s2 = valb > 0 ? 1 : (valb == 0 ? 0 : -1);
    const auto vala2 = static_cast<ExtNum>(vala) * vala;
    const auto valb2 = static_cast<ExtNum>(valb) * valb;
    const auto r2 = static_cast<ExtNum>(radius) * radius;
    const auto d2 = static_cast<ExtNum>(angle.m_x) * angle.m_x + static_cast<ExtNum>(angle.m_y) * angle.m_y;
    if (circle_pt_compare_less(linep1.m_y, center.m_y, radius, angle.m_y, angle.m_x)) {
        if (pt.m_y < linep1.m_y || !circle_pt_compare_less(pt.m_y, center.m_y, radius, angle.m_y, angle.m_x)) {
            return 0;
        }
        const auto inLeftVal = inLeft();
        if (inLeftVal.has_value()) {
            if (inLeftVal.value()) {
                return 1;
            } else {
                return 0;
            }
        }
        if (s1 != s2) {
            return s1 > s2;
        }
        if (s1 < 0) {
            return compare_aob2cod(vala2, r2, valb2, d2) ? 1 : 0;
        } else if (s1 == 0) {
            return 0;
        } else {
            return compare_aob2cod(valb2, d2, vala2, r2) ? 1 : 0;
        }
    } else if (circle_pt_compare_greater(linep1.m_y, center.m_y, radius, angle.m_y, angle.m_x)) {
        if (pt.m_y >= linep1.m_y || circle_pt_compare_less(pt.m_y, center.m_y, radius, angle.m_y, angle.m_x)) {
            return 0;
        }
        const auto inLeftVal = inLeft();
        if (inLeftVal.has_value()) {
            if (inLeftVal.value()) {
                return 1;
            } else {
                return 0;
            }
        }
        if (s1 < 0) {
            return compare_aob2cod(valb2, d2, vala2, r2) ? 1 : 0;
        } else if (s1 == 0) {
            return false;
        } else {
            return compare_aob2cod(vala2, r2, valb2, d2) ? 1 : 0;
        }
    } else {
        return 0;
    }
}

template<typename NUM>
size_t rayCastToArcSegmentFixAbove(const Point<NUM>& pt, const ARC_SEGMENT_DATA<NUM>& arc)
{
    using ExtNum = typename NumTraits<NUM>::ExtendType;
    auto s1 = rayCastToArcSegmentAbove(pt, arc);
    if (static_cast<ExtNum>(arc.m_radius) * arc.m_radius != (arc.m_fromPt - arc.m_center).SquaredEuclideanNorm()) {
        s1 += rayCastToLineSegmentArcPointAbove(pt, arc.m_fromPt, arc.m_radius, arc.m_center, arc.m_range.m_from);
    }
    if (static_cast<ExtNum>(arc.m_radius) * arc.m_radius != (arc.m_toPt - arc.m_center).SquaredEuclideanNorm()) {
        s1 += rayCastToLineSegmentArcPointAbove(pt, arc.m_toPt, arc.m_radius, arc.m_center, arc.m_range.m_to);
    }
    return s1;
}

template<typename SH>
struct SHAPE_POINT_TRAIT { };
template<typename SH, typename = void>
struct has_shape_ap: std::false_type {};
template<typename SH>
struct has_shape_ap<SH, std::void_t<decltype(SHAPE_POINT_TRAIT<SH>::getAP(std::declval<const SH&>()))>>: std::true_type {};
template<typename SH>
constexpr bool has_shape_ap_v = has_shape_ap<SH>::value;

template<typename NUM>
struct SHAPE_POINT_TRAIT<Point<NUM>>
{
    static Point<NUM> getAP(const Point<NUM>& pt) { return pt; };
};
template<typename NUM>
struct SHAPE_POINT_TRAIT<SEGMENT_DATA<NUM>>
{
    static Point<NUM> getAP(const SEGMENT_DATA<NUM>& seg) { return seg.m_a; };
};
template<typename NUM>
struct SHAPE_POINT_TRAIT<ARC_SEGMENT_DATA<NUM>>
{
    static Point<NUM> getAP(const ARC_SEGMENT_DATA<NUM>& seg) { return seg.p1(); };
};
template<typename NUM>
struct SHAPE_POINT_TRAIT<CIRCLE_DATA<NUM>>
{
    static Point<NUM> getAP(const CIRCLE_DATA<NUM>& circle) { return circle.m_center + Point<NUM>(circle.m_radius, 0); };
};
template<typename NUM>
struct SHAPE_POINT_TRAIT<POLYGON_DATA<NUM>>
{
    static Point<NUM> getAP(const POLYGON_DATA<NUM>& polygon) { H2GAssert(!polygon.m_points.empty()); return polygon.m_points.front(); };
};
template<typename NUM>
struct SHAPE_POINT_TRAIT<COMPLEX_POLYGON_DATA<NUM>>
{
    static Point<NUM> getAP(const COMPLEX_POLYGON_DATA<NUM>& polygon) {
        H2GAssert(!polygon.m_points.empty());
        return polygon.m_points.front().point();
    }
};


template<typename NUM>
bool pointInside(const POLYGON_DATA<NUM>& polygon, const Point<NUM>& pt)
{
    H2GAssert(polygon.m_points.size() > 1);
    size_t count = 0;
    for (size_t i=0;i<polygon.m_points.size();i++) {
        const auto p1 = polygon.m_points.at(i);
        const auto p2 = polygon.m_points.at((i+1) % polygon.m_points.size());
        count += rayCastToLineSegmentAbove(pt, SEGMENT_DATA<NUM>(p1, p2));
    }
    return count % 2 == 1;
}

template<typename NUM>
bool pointInside(const COMPLEX_POLYGON_DATA<NUM>& polygon, const Point<NUM>& pt)
{
    H2GAssert(polygon.m_points.size() > 1);
    size_t count = 0;
    for (size_t i=0;i<polygon.m_points.size();i++) {
        const auto n1 = polygon.m_points.at(i);
        const auto p2 = polygon.m_points.at((i+1) % polygon.m_points.size()).m_normal.m_point;
        if (n1.m_normal.m_type == POLYGON_NODE_TYPE::NORMAL) {
            count += rayCastToLineSegmentAbove(pt, SEGMENT_DATA<NUM>(n1.m_normal.m_point, p2));
        } else if (n1.m_normal.m_type == POLYGON_NODE_TYPE::ARC) {
            count += rayCastToArcSegmentFixAbove(pt, ARC_SEGMENT_DATA<NUM>(n1.m_arc.m_point, p2, n1.m_arc.m_radius, n1.m_arc.m_cclockwise));
        } else {
            H2GUnreachable();
        }
    }
    return count % 2 == 1;
}

template<typename NUM, typename SH>
struct SHAPE_DISTANCE_TRAIT<NUM,POLYGON_DATA<NUM>,SH,SC_TAG_DEFAULT,
                            std::enable_if_t<has_shape_ap_v<SH>>> {
    static NUM distance_impl(const POLYGON_DATA<NUM>& polygon, const SH& oth, _NearestPts<NUM>& nearestPts)
    {
        const auto ap = SHAPE_POINT_TRAIT<SH>::getAP(oth);
        if (pointInside(polygon, ap)) {
            nearestPts = {ap, ap};
            return NUM(0);
        }

        std::optional<NUM> ans;
        for (size_t i=0;i<polygon.m_points.size();i++) {
            const auto p1 = polygon.m_points.at(i);
            const auto p2 = polygon.m_points.at((i+1) % polygon.m_points.size());
            auto pts = nearestPts;
            const auto dis = distance_dispatcher(SEGMENT_DATA<NUM>(p1, p2), oth, pts);
            if (!ans.has_value() || closerThan(pts, nearestPts)) {
                ans = dis;
                nearestPts = pts;
            }
        }

        return ans.value();
    }
};

template<typename NUM, typename SH>
struct SHAPE_DISTANCE_TRAIT<NUM,COMPLEX_POLYGON_DATA<NUM>,SH,SC_TAG_DEFAULT,
                            std::enable_if_t<has_shape_ap_v<SH>>>
{
    static NUM distance_impl(const COMPLEX_POLYGON_DATA<NUM>& polygon, const SH& oth, _NearestPts<NUM>& nearestPts)
    {
        const auto ap = SHAPE_POINT_TRAIT<SH>::getAP(oth);
        if (pointInside(polygon, ap)) {
            nearestPts = {ap, ap};
            return NUM(0);
        }

        std::optional<NUM> ans;
        for (size_t i=0;i<polygon.m_points.size();i++) {
            const auto p1 = polygon.m_points.at(i);
            const auto p2 = polygon.m_points.at((i+1) % polygon.m_points.size());
            auto pts = nearestPts;
            std::optional<NUM> dis;
            if (p1.m_normal.m_type == POLYGON_NODE_TYPE::NORMAL) {
                dis = distance_dispatcher(SEGMENT_DATA<NUM>(p1.m_normal.m_point, p2.m_normal.m_point), oth, pts);
            } else if (p1.m_normal.m_type == POLYGON_NODE_TYPE::ARC) {
                dis = distance_dispatcher(ARC_SEGMENT_DATA<NUM>(p1.m_arc.m_point, p2.m_normal.m_point, p2.m_arc.m_radius, p2.m_arc.m_cclockwise), oth, pts);
            }
            if (!ans.has_value() || closerThan(pts, nearestPts)) {
                ans = dis;
                nearestPts = pts;
            }
        }

        return ans.value();
    }
};

template<typename SH, typename NUM>
inline NUM distance_impl_helper(const SH& sh, const SHAPE<NUM>& oth, std::pair<Point<NUM>,Point<NUM>>& pts)
{
    switch (oth.type()) {
    case SHAPE_TYPE::SEGMENT:
        return distance_dispatcher(sh, oth.asSegment().data(), pts);
    case SHAPE_TYPE::ARC_SEGMENT:
        return distance_dispatcher(sh, oth.asArc().data(), pts);
    case SHAPE_TYPE::CIRCLE:
        return distance_dispatcher(sh, oth.asCircle().data(), pts);
    case SHAPE_TYPE::POLYGON:
        return distance_dispatcher(sh, oth.asPolygon().data(), pts);
    case SHAPE_TYPE::COMPLEX_POLYGON:
        return distance_dispatcher(sh, oth.asComplexPolygon().data(), pts);
    default:
        H2GUnreachable();
    }
    return NUM(0);
}

template<typename NUM>
NUM SHAPE<NUM>::distance(const SHAPE<NUM>& oth, std::pair<Point<NUM>,Point<NUM>>& pts) const
{
    switch (this->type()) {
    case SHAPE_TYPE::SEGMENT:
        return distance_impl_helper(asSegment().data(), oth, pts);
    case SHAPE_TYPE::ARC_SEGMENT:
        return distance_impl_helper(asArc().data(), oth, pts);
    case SHAPE_TYPE::CIRCLE:
        return distance_impl_helper(asCircle().data(), oth, pts);
    case SHAPE_TYPE::POLYGON:
        return distance_impl_helper(asPolygon().data(), oth, pts);
    case SHAPE_TYPE::COMPLEX_POLYGON:
        return distance_impl_helper(asComplexPolygon().data(), oth, pts);
    default:
        H2GUnreachable();
    }
    return NUM(0);
}

}


