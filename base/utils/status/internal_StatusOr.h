#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

#ifndef LINUXGAMESERVER_INTERNAL_STATUSOR_H
#define LINUXGAMESERVER_INTERNAL_STATUSOR_H


#include"Status.h"
#include "StatusOr.hpp"
#include<type_traits>
#include <exception>

namespace yy::util::internal_statusor {


template <typename T> class StatusOr;




// Detects whether `U` has conversion operator to `StatusOr<T>`, i.e. `operator
// StatusOr<T>()`.
template <typename T, typename U, typename = void>
struct HasConversionOperatorToStatusOr : std::false_type {};

template <typename T, typename U>
void test(char (*)[sizeof(std::declval<U>().operator StatusOr<T>())]);

template <typename T, typename U>
struct HasConversionOperatorToStatusOr<T, U, decltype(test<T, U>(0))> : std::true_type {};





/************************ Converting Constructors ***********************/
// Detects whether `T` is constructible or convertible from `StatusOr<U>`.
// 判断T和能否从StatusOr<U>（construct| convert），即StatusOr<U>& ==> T
template <typename T, typename U>
using IsConstructibleOrConvertibleFromStatusOr =
        std::disjunction< std::is_constructible<T, StatusOr<U>&>,
                std::is_constructible<T, const StatusOr<U>&>,
                std::is_constructible<T, StatusOr<U>&&>,
                std::is_constructible<T, const StatusOr<U>&&>,
                std::is_convertible<StatusOr<U>&, T>,
                std::is_convertible<const StatusOr<U>&, T>,
                std::is_convertible<StatusOr<U>&&, T>,
                std::is_convertible<const StatusOr<U>&&, T>>;


// Detects whether `T` is constructible or convertible or assignable from `StatusOr<U>`.
// 判断T和能否从StatusOr<U>（construct| convert | assign）
template <typename T, typename U>
using IsConstructibleOrConvertibleOrAssignableFromStatusOr =
        std::disjunction< IsConstructibleOrConvertibleFromStatusOr<T, U>,
                std::is_assignable<T&, StatusOr<U>&>,
                std::is_assignable<T&, const StatusOr<U>&>,
                std::is_assignable<T&, StatusOr<U>&&>,
                std::is_assignable<T&, const StatusOr<U>&&>>;


template <typename T, typename U>
concept IsConstructibleOrConvertibleFromStatusOrConcept = requires {
    std::is_constructible_v<T, StatusOr<U>&> ||
    std::is_constructible_v<T, const StatusOr<U>&> ||
    std::is_constructible_v<T, StatusOr<U>&&> ||
    std::is_constructible_v<T, const StatusOr<U>&&> ||
    std::is_convertible_v<StatusOr<U>&, T> ||
    std::is_convertible_v<const StatusOr<U>&, T> ||
    std::is_convertible_v<StatusOr<U>&&, T> ||
    std::is_convertible_v<const StatusOr<U>&&, T>;
};


template <typename T, typename U>
concept IsAssignableFromStatusOrConcept = requires {
    std::is_assignable_v<T&, StatusOr<U>&> ||
    std::is_assignable_v<T&, const StatusOr<U>&> ||
    std::is_assignable_v<T&, StatusOr<U>&&> ||
    std::is_assignable_v<T&, const StatusOr<U>&&>;
};




/************************ Is Direct Initialization ***********************/
// Detects whether direct initializing `StatusOr<T>` from `U` is ambiguous, i.e.
// when `U` is `StatusOr<V>` and `T` is constructible or convertible from `V`.
template <typename T, typename U>
struct IsDirectInitializationAmbiguous
        : public std::conditional_t<
                std::is_same_v<std::remove_cv_t<std::remove_reference_t<U>>, U>,
/* if true  */      std::false_type,
/* if false */      IsDirectInitializationAmbiguous< T, std::remove_cv_t<std::remove_reference_t<U>>>> {};
// 特化
template <typename T, typename V>
struct IsDirectInitializationAmbiguous<T, StatusOr<V>>
        : public IsConstructibleOrConvertibleFromStatusOr<T, V> {};

// Checks against the constraints of the direction initialization, i.e. when
// `StatusOr<T>::StatusOr(U&&)` should participate in overload resolution.
template <typename T, typename U>
using IsDirectInitializationValid =
        std::disjunction<
                // Short circuits if T is basically U.
                std::is_same<T, std::remove_cv_t<std::remove_reference_t<U>>>,
                std::negation<
                        std::disjunction<
                                std::is_same<StatusOr<T>    , std::remove_cv_t<std::remove_reference_t<U>>>,
                                std::is_same<Status         , std::remove_cv_t<std::remove_reference_t<U>>>,
                                std::is_same<std::in_place_t, std::remove_cv_t<std::remove_reference_t<U>>>,
                                IsDirectInitializationAmbiguous<T, U>>>>;





/************************ IsForwardingAssignmentValid ***********************/
// This trait detects whether `StatusOr<T>::operator=(U&&)` is ambiguous, which
// is equivalent to whether all the following conditions are met:
// 1. `U` is `StatusOr<V>`.
// 2. `T` is constructible and assignable from `V`.
// 3. `T` is constructible and assignable from `U` (i.e. `StatusOr<V>`).
// For example, the following code is considered ambiguous:
// (`T` is `bool`, `U` is `StatusOr<bool>`, `V` is `bool`)
//   StatusOr<bool> s1 = true;  // s1.ok() && s1.ValueOrDie() == true
//   StatusOr<bool> s2 = false;  // s2.ok() && s2.ValueOrDie() == false
//   s1 = s2;  // ambiguous, `s1 = s2.ValueOrDie()` or `s1 = bool(s2)`?
template <typename T, typename U>
struct IsForwardingAssignmentAmbiguous
        : public std::conditional_t <
                std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, U>::value,
/* if true  */      std::false_type,
/* if false */      IsForwardingAssignmentAmbiguous<T, std::remove_cv_t<std::remove_reference_t<U>>>> {};

template <typename T, typename U>
struct IsForwardingAssignmentAmbiguous<T, StatusOr<U>>
        : public IsConstructibleOrConvertibleOrAssignableFromStatusOr<T, U> {};

// Checks against the constraints of the forwarding assignment, i.e. whether
// `StatusOr<T>::operator(U&&)` should participate in overload resolution.
template <typename T, typename U>
using IsForwardingAssignmentValid =
        std::disjunction<
                // Short circuits if T is basically U.
                std::is_same<T, std::remove_cv_t<std::remove_reference_t<U>>>,
                std::negation<
                        std::disjunction<
                                std::is_same<StatusOr<T>    , std::remove_cv_t<std::remove_reference_t<U>>>,
                                std::is_same<Status         , std::remove_cv_t<std::remove_reference_t<U>>>,
                                std::is_same<std::in_place_t, std::remove_cv_t<std::remove_reference_t<U>>>,
                                IsForwardingAssignmentAmbiguous<T, U>>>>;










// Construct an instance of T in `p` through placement new, passing Args... to the constructor.
// This abstraction is here mostly for the gcc performance fix.
template <typename T, typename... Args>
void PlacementNew(void* p, Args&&... args) {
    new (p) T(std::forward<Args>(args)...);
}




void ThrowBadStatusOrAccess(Status status) { //NOLINT
#ifdef YY_HAVE_EXCEPTIONS
    throw BadStatusOrAccess(std::move(status));
#else
    std::cerr << ("Attempting to fetch value instead of handling error " + status.ToString()) ;
    std::abort();
#endif
}



template <typename T>
class StatusOrData
{
    template<typename U>
    friend class StatusOrData;
public:
    //todo



    StatusOrData() = delete;

    StatusOrData(const StatusOrData& other) {
        if (other.ok()) {
            MakeValue(other.data_);
            MakeStatus();
        } else {
            MakeStatus(other.status_);
        }
    }

    StatusOrData(StatusOrData&& other) noexcept {
        if (other.ok()) {
            MakeValue(std::move(other.data_));
            MakeStatus();
        } else {
            MakeStatus(std::move(other.status_));
        }
    }

    template <typename U>
    explicit StatusOrData(const StatusOrData<U>& other) {
        if (other.ok()) {
            MakeValue(other.data_);
            MakeStatus();
        } else {
            MakeStatus(other.status_);
        }
    }

    template <typename U>
    explicit StatusOrData(StatusOrData<U>&& other) {
        if (other.ok()) {
            MakeValue(std::move(other.data_));
            MakeStatus();
        } else {
            MakeStatus(std::move(other.status_));
        }
    }

    template <typename... Args>
    explicit StatusOrData(std::in_place_t, Args&&... args)
            : data_(std::forward<Args>(args)...) {
        MakeStatus();
    }

    explicit StatusOrData(const T& value) : data_(value) {
        MakeStatus();
    }
    explicit StatusOrData(T&& value) : data_(std::move(value)) {
        MakeStatus();
    }

    template <typename U>
    requires std::is_constructible_v<Status, U&&>
    explicit StatusOrData(U&& v) : status_(std::forward<U>(v)) {
        EnsureNotOk();
    }

    StatusOrData& operator=(const StatusOrData& other) {
        if (this == &other) return *this;
        if (other.ok())
            this->Assign(other.data_);
        else
            this->AssignStatus(other.status_);
        return *this;
    }

    StatusOrData& operator=(StatusOrData&& other)  noexcept {
        if (this == &other) return *this;
        if (other.ok())
            this->Assign(std::move(other.data_));
        else
            this->AssignStatus(std::move(other.status_));
        return *this;
    }

    ~StatusOrData() {
        if (ok()) {
            status_.~Status();
            data_.~T();
        } else {
            status_.~Status();
        }
    }



    /******************** 辅助函数 *******************/
    template <typename U>
    void Assign(U&& value) {
        if (ok()) {
            data_ = std::forward<U>(value);
        } else {
            MakeValue(std::forward<U>(value));
            status_ = OkStatus();
        }
    }

    template <typename U>
    void AssignStatus(U&& v) {
        this->Clear();
        status_ = static_cast<Status>(std::forward<U>(v));
        EnsureNotOk();
    }

    [[nodiscard]] bool ok() const { return status_.ok(); }

protected:
    void Clear() {
        if (this->ok())
            data_.~T();
    }

    void EnsureOk() {
        if (!this->ok()) {
            ThrowBadStatusOrAccess(this->status_);
        }
    }

    void EnsureNotOk() {
        if (this->ok()) {
            ThrowBadStatusOrAccess(this->status_);
        }
    }

    // 通过placement new构造data_
    template <typename... Arg>
    void MakeValue(Arg&&... arg) {
        PlacementNew<T>(&dummy_, std::forward<Arg>(arg)...);
    }

    // 通过placement new构造status_
    template <typename... Args>
    void MakeStatus(Args&&... args) {
        PlacementNew<Status>(&status_, std::forward<Args>(args)...);
    }




    Status status_;

    struct Dummy { };
    union  {
        // When T is const, we need some non-const object we can cast to void*
        // for the placement new. dummy_ is that object.
        Dummy dummy_;

        // data_ is active iff status_.ok()==true
        T data_;
    };


};







/************ 让StatusOr<T>和T的构造/赋值函数同步的辅助类 ———— 「你有我也有，你没有我也没有」 ************/
// StatusOr<T>继承这些类，当T没有某构造/赋值函数时，StatusOr<T>也将没有
template <typename T, bool = std::is_copy_constructible_v<T>>
struct CopyCtorBase {
    CopyCtorBase() = default;
    CopyCtorBase(const CopyCtorBase&) = default;
    CopyCtorBase(CopyCtorBase&&)  noexcept = default;
    CopyCtorBase& operator=(const CopyCtorBase&) = default;
    CopyCtorBase& operator=(CopyCtorBase&&) noexcept = default;
};
// 模板第二参数推断为false时，即类T不含有该构造/赋值函数时，删除该函数（其他的也是一样）
template <typename T>
struct CopyCtorBase<T, false> {
    CopyCtorBase() = default;
    CopyCtorBase(const CopyCtorBase&) = delete;
    CopyCtorBase(CopyCtorBase&&)  noexcept = default;
    CopyCtorBase& operator=(const CopyCtorBase&) = default;
    CopyCtorBase& operator=(CopyCtorBase&&) noexcept = default;
};

template <typename T, bool = std::is_move_constructible_v<T>>
struct MoveCtorBase {
    MoveCtorBase() = default;
    MoveCtorBase(const MoveCtorBase&) = default;
    MoveCtorBase(MoveCtorBase&&)  noexcept = default;
    MoveCtorBase& operator=(const MoveCtorBase&) = default;
    MoveCtorBase& operator=(MoveCtorBase&&) noexcept = default;
};
template <typename T>
struct MoveCtorBase<T, false> {
    MoveCtorBase() = default;
    MoveCtorBase(const MoveCtorBase&) = default;
    MoveCtorBase(MoveCtorBase&&) = delete;
    MoveCtorBase& operator=(const MoveCtorBase&) = default;
    MoveCtorBase& operator=(MoveCtorBase&&) noexcept = default;
};

template <typename T, bool = std::is_copy_constructible_v<T>&& std::is_copy_assignable_v<T>>
struct CopyAssignBase {
    CopyAssignBase() = default;
    CopyAssignBase(const CopyAssignBase&) = default;
    CopyAssignBase(CopyAssignBase&&)  noexcept = default;
    CopyAssignBase& operator=(const CopyAssignBase&) = default;
    CopyAssignBase& operator=(CopyAssignBase&&) noexcept = default;
};
template <typename T>
struct CopyAssignBase<T, false> {
    CopyAssignBase() = default;
    CopyAssignBase(const CopyAssignBase&) = default;
    CopyAssignBase(CopyAssignBase&&)  noexcept = default;
    CopyAssignBase& operator=(const CopyAssignBase&) = delete;
    CopyAssignBase& operator=(CopyAssignBase&&) noexcept = default;
};

template <typename T, bool = std::is_move_constructible_v<T> && std::is_move_assignable_v<T>>
struct MoveAssignBase {
    MoveAssignBase() = default;
    MoveAssignBase(const MoveAssignBase&) = default;
    MoveAssignBase(MoveAssignBase&&)  noexcept = default;
    MoveAssignBase& operator=(const MoveAssignBase&) = default;
    MoveAssignBase& operator=(MoveAssignBase&&) noexcept = default;
};
template <typename T>
struct MoveAssignBase<T, false> {
    MoveAssignBase() = default;
    MoveAssignBase(const MoveAssignBase&) = default;
    MoveAssignBase(MoveAssignBase&&)  noexcept = default;
    MoveAssignBase& operator=(const MoveAssignBase&) = default;
    MoveAssignBase& operator=(MoveAssignBase&&) noexcept = delete;
};




}





#pragma clang diagnostic pop







#endif //LINUXGAMESERVER_INTERNAL_STATUSOR_H
