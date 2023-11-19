#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#ifndef ____STATUSOR_HPP
#define ____STATUSOR_HPP

#include <utility>
#include <system_error>
#include <type_traits>
#include <mutex>
#include <iostream>

#include "internal_StatusOr.h"
#include "Status.h"


namespace yy::util {






class BadStatusOrAccess : public std::exception {
public:
    explicit BadStatusOrAccess(Status status)
        : status_(std::move(status)) {}
    ~BadStatusOrAccess() override = default;

    BadStatusOrAccess(const BadStatusOrAccess& other)
        : status_(other.status_) {}


    BadStatusOrAccess& operator=(const BadStatusOrAccess& other)
    {
        // Ensure assignment is correct regardless of whether this->InitWhat() has already been called.
        other.InitWhat();
        status_ = other.status_;
        what_ = other.what_;
        return *this;
    }


    BadStatusOrAccess(BadStatusOrAccess&& other) noexcept
          : status_(std::move(other.status_)) {}


    BadStatusOrAccess& operator=(BadStatusOrAccess&& other) noexcept
    {
        // Ensure assignment is correct regardless of whether this->InitWhat() has
        // already been called.
        other.InitWhat();
        status_ = std::move(other.status_);
        what_ = std::move(other.what_);
        return *this;
    }

    const char* what() const noexcept override
    {
        InitWhat();
        return what_.c_str();
    }

    // Returns the associated `Status` of the `StatusOr<T>` object's error.
    const Status& status() const { return status_; }

private:
    void InitWhat() const
    {
        std::call_once(init_what_, [this] {
            what_ = "Bad StatusOr access: " + status_.ToString();
        });
    }

    Status status_;
    mutable std::once_flag init_what_;
    mutable std::string what_;
};









template<typename T>
class StatusOr final: private internal_statusor::StatusOrData<T>, // 必须第一个继承
                      private internal_statusor::CopyCtorBase<T>,
                      private internal_statusor::MoveCtorBase<T>,
                      private internal_statusor::CopyAssignBase<T>,
                      private internal_statusor::MoveAssignBase<T>
{
    template<typename U>
    friend
    class StatusOr;

    using Base = internal_statusor::StatusOrData<T>;

public:
    using value_type = T;
public:
    /********** Constructors **********/
    explicit StatusOr() : Base(Status(StatusCode::kUnknown, "")) {}

    // copy
    StatusOr(const StatusOr&) = default;
    StatusOr& operator=(const StatusOr& obj) = default;
    // move
    StatusOr(StatusOr&& obj) noexcept = default;
    StatusOr& operator=(StatusOr&& obj) noexcept = default;
    ~StatusOr() = default;




    /********** Converting Constructors **********/
    /*
     * 当T可从U构造时，支持StatusOr<T>也可从StatusOr<U>构造。
     * 为了避免二义性，如果T也能从StatusOr<U>构造，以下的构造函数将会被禁用。
     * 从T到U的相应构造函数是explicit <==> StatusOr<T>的构造函数也是explicit。
     * （也就是说StatusOr<T>的构造函数继承了其内部类型T的explicit）
     */
    // &
#if __cplusplus <= 201703L
    template <
            typename U,
            std::enable_if_t<
                    std::conjunction<
                            std::negation<std::is_same<T, U>>,
                            std::is_constructible<T, const U&>,
                            std::is_convertible<const U&, T>,
                            std::negation< internal_statusor::IsConstructibleOrConvertibleFromStatusOr< T, U>>>::value,
                    int> = 0>
#else
    template<typename U>
    requires requires {
        requires !std::is_same_v<T, U>;
        requires  std::is_constructible_v<T, const U&>;
        requires  std::is_convertible_v<const U&, T>;
        requires !internal_statusor::IsConstructibleOrConvertibleFromStatusOr<T, U>::value;
    }
#endif
    StatusOr(const StatusOr<U>& other) // NOLINT
            : Base(static_cast<const typename StatusOr<U>::Base&>(other)) {}

#if __cplusplus <= 201703L
    template <
            typename U,
            std::enable_if_t<
                    std::conjunction<
                            std::negation<std::is_same<T, U>>,
                            std::is_constructible<T, const U&>,
                            std::negation<std::is_convertible<const U&, T>>,
                            std::negation< internal_statusor::IsConstructibleOrConvertibleFromStatusOr< T, U>>>::value,
                    int> = 0>
#else
    template<typename U>
    requires requires {
        requires !std::is_same_v<T, U>;
        requires  std::is_constructible_v<T, const U&>;
        requires !std::is_convertible_v<const U&, T>;
        requires !internal_statusor::IsConstructibleOrConvertibleFromStatusOr<T, U>::value;
    }
#endif
    explicit StatusOr(const StatusOr<U>& other)
            : Base(static_cast<const typename StatusOr<U>::Base&>(other)) {}

    //&&
#if __cplusplus <= 201703L
    template <
            typename U,
            std::enable_if_t<
                    std::conjunction<
                            std::negation<std::is_same<T, U>>,
                            std::is_constructible<T, U&&>,
                            std::is_convertible<U&&, T>,
                            std::negation<internal_statusor::IsConstructibleOrConvertibleFromStatusOr<T, U>>>::value,
                    int> = 0>
#else
    template<typename U>
    requires requires {
        requires !std::is_same_v<T, U>;
        requires  std::is_constructible_v<T, U&&>;
        requires  std::is_convertible_v<U&&, T>;
        requires !internal_statusor::IsConstructibleOrConvertibleFromStatusOr<T, U>::value;
    }
#endif
    StatusOr(StatusOr<U>&& other)  // NOLINT
            : Base(static_cast<typename StatusOr<U>::Base&&>(other)) {}

#if __cplusplus <= 201703L
    template <
            typename U,
            std::enable_if_t<
                    std::conjunction<
                            std::negation<std::is_same<T, U>>,
                            std::is_constructible<T, U&&>,
                            std::negation<std::is_convertible<U&&, T>>,
                            std::negation< internal_statusor::IsConstructibleOrConvertibleFromStatusOr<T, U>>>::value,
                    int> = 0>
#else
    template<typename U>
    requires requires {
        requires !std::is_same_v<T, U>;
        requires  std::is_constructible_v<T, U&&>;
        requires !std::is_convertible_v<U&&, T>;
        requires !internal_statusor::IsConstructibleOrConvertibleFromStatusOr<T, U>::value;
    }
#endif
    explicit StatusOr(StatusOr<U>&& other)
            : Base(static_cast<typename StatusOr<U>::Base&&>(other)) {}



    /******************** Converting Assignment Operators ********************/
    /*
     * 当情况全部发生时，从StatusOr<U>赋值创建一个StatusOr<T>：
     *
     *        | StatusOr<T>   |  StatusOr<U>  |
     *        |     OK        |      OK       |   通过T=U
     *        |     OK        |  error code   |   通过销毁StatusOr<T>的值并从StatusOr<U>赋值
     *        | error code    |      OK       |   直接从U初始化T
     *        | error code    |  error code   |   将StatusOr<U>中的Status赋值给StatusOr<T>
     *
     * 仅当StatusOr<T>可从StatusOr<U>构造和赋值 并且 StatusOr<T>不能直接从StatusOr<U>赋值
     * */

    //&
#if __cplusplus <= 201703L
    template <
            typename U,
            std::enable_if_t<
                    std::conjunction<
                            std::negation<std::is_same<T, U>>,
                            std::is_constructible<T, const U&>,
                            std::is_assignable<T, const U&>,
                            std::negation< internal_statusor::IsConstructibleOrConvertibleOrAssignableFromStatusOr<T, U>>>::value,
                    int> = 0>
#else
    template<typename U>
    requires requires {
        requires !std::is_same_v<T, U>;
        requires  std::is_constructible_v<T, const U&>;
        requires  std::is_assignable_v<T, const U&>;
        requires !internal_statusor::IsConstructibleOrConvertibleOrAssignableFromStatusOr<T, U>::value;
    }
#endif
    StatusOr& operator=(const StatusOr<U>& other) {
        this->Assign(other);
        return *this;
    }

    // &&
#if __cplusplus <= 201703L
    template <
            typename U,
            std::enable_if_t<
                    std::conjunction<
                            std::negation<std::is_same<T, U>>,
                            std::is_constructible<T, U&&>,
                            std::is_assignable<T, U&&>,
                            std::negation<internal_statusor::IsConstructibleOrConvertibleOrAssignableFromStatusOr<T, U>>>::value,
                    int> = 0>
#else
    template<typename U>
    requires requires {
        requires !std::is_same_v<T, U>;
        requires  std::is_constructible_v<T, U&&>;
        requires  std::is_assignable_v<T, U&&>;
        requires !internal_statusor::IsConstructibleOrConvertibleOrAssignableFromStatusOr<T, U>::value;
    }
#endif
    StatusOr& operator=(StatusOr<U>&& other) {
        this->Assign(std::move(other));
        return *this;
    }






    /******************** HasConversionOperatorToStatusOr ********************/
    /*
     *
     * */
    // Constructs a new `StatusOr<T>` with a non-ok status. After calling
    // this constructor, `this->ok()` will be `false` and calls to `value()` will
    // crash, or produce an exception if exceptions are enabled.
    //
    // The constructor also takes any type `U` that is convertible to
    // `Status`. This constructor is explicit if and only if `U` is not of
    // type `Status` and the conversion from `U` to `Status` is explicit.
    //
    // REQUIRES: !Status(std::forward<U>(v)).ok(). This requirement is DCHECKed.
    // In optimized builds, passing std::OkStatus() here will have the effect
    // of passing StatusCode::kInternal as a fallback.

#if __cplusplus <= 201703L
    template <
            typename U = Status,
            std::enable_if_t<
                    std::conjunction<
                            std::is_convertible<U&&, Status>,
                            std::is_constructible<Status, U&&>,
                            std::negation<std::is_same<std::decay_t<U>, StatusOr<T>>>,
                            std::negation<std::is_same<std::decay_t<U>, T>>,
                            std::negation<std::is_same<std::decay_t<U>, std::in_place_t>>,
                            std::negation<internal_statusor::HasConversionOperatorToStatusOr<T, U&&>>>::value,
                    int> = 0>
#else
    template <typename U = Status>
    requires requires {
        requires  std::is_convertible_v<U&&, Status>;
        requires  std::is_constructible_v<Status, U&&>;
        requires !std::is_same_v<std::decay_t<U>, StatusOr<T>>;
        requires !std::is_same_v<std::decay_t<U>, T>;
        requires !std::is_same_v<std::decay_t<U>, std::in_place_t>;
        requires !internal_statusor::HasConversionOperatorToStatusOr<T, U&&>::value;
    }
#endif
    StatusOr(U&& v) : Base(std::forward<U>(v)) {} // NOLINT

#if __cplusplus <= 201703L
    template <
            typename U = Status,
            std::enable_if_t<
                    std::conjunction<
                            std::negation<std::is_convertible<U&&, Status>>,
                            std::is_constructible<Status, U&&>,
                            std::negation<std::is_same<std::decay_t<U>, StatusOr<T>>>,
                            std::negation<std::is_same<std::decay_t<U>, T>>,
                            std::negation<std::is_same<std::decay_t<U>, std::in_place_t>>,
                            std::negation<internal_statusor::HasConversionOperatorToStatusOr<T, U&&>>>::value,
                    int> = 0>
#else
    template <typename U = Status>
    requires requires {
        requires !std::is_convertible_v<U&&, Status>;
        requires  std::is_constructible_v<Status, U&&>;
        requires !std::is_same_v<std::decay_t<U>, StatusOr<T>>;
        requires !std::is_same_v<std::decay_t<U>, T>;
        requires !std::is_same_v<std::decay_t<U>, std::in_place_t>;
        requires !internal_statusor::HasConversionOperatorToStatusOr<T, U&&>::value;
    }
#endif
    explicit StatusOr(U&& v) : Base(std::forward<U>(v)) {} //NOLINT



#if __cplusplus <= 201703L
    template <
            typename U = Status,
            std::enable_if_t<
                    std::conjunction<
                            std::is_convertible<U&&, Status>,
                            std::is_constructible<Status, U&&>,
                            std::negation<std::is_same<std::decay_t<U>, StatusOr<T>>>,
                            std::negation<std::is_same<std::decay_t<U>, T>>,
                            std::negation<std::is_same<std::decay_t<U>, std::in_place_t>>,
                            std::negation<internal_statusor::HasConversionOperatorToStatusOr<T, U&&>>>::value,
                    int> = 0>
#else
    template <typename U = Status>
    requires requires {
        requires  std::is_convertible_v<U&&, Status>;
        requires  std::is_constructible_v<Status, U&&>;
        requires !std::is_same_v<std::decay_t<U>, StatusOr<T>>;
        requires !std::is_same_v<std::decay_t<U>, T>;
        requires !std::is_same_v<std::decay_t<U>, std::in_place_t>;
        requires !internal_statusor::HasConversionOperatorToStatusOr<T, U&&>::value;
    }
#endif
    StatusOr& operator=(U&& v) {
        this->AssignStatus(std::forward<U>(v));
        return *this;
    }




    /******************** Perfect-forwarding value assignment operator ********************/
    // If `*this` contains a `T` value before the call, the contained value is
    // assigned from `std::forward<U>(v)`; Otherwise, it is directly-initialized
    // from `std::forward<U>(v)`.
    // This function does not participate in overload unless:
    // 1. `std::is_constructible_v<T, U>` is true,
    // 2. `std::is_assignable_v<T&, U>` is true.
    // 3. `std::is_same_v<StatusOr<T>, std::remove_cvref_t<U>>` is false.
    // 4. Assigning `U` to `T` is not ambiguous:
    //  If `U` is `StatusOr<V>` and `T` is constructible and assignable from
    //  both `StatusOr<V>` and `V`, the assignment is considered bug-prone and
    //  ambiguous thus will fail to compile. For example:
    //    StatusOr<bool> s1 = true;  // s1.ok() && *s1 == true
    //    StatusOr<bool> s2 = false;  // s2.ok() && *s2 == false
    //    s1 = s2;  // ambiguous, `s1 = *s2` or `s1 = bool(s2)`?
#if __cplusplus <= 201703L
    template <typename U = T,
            typename = typename std::enable_if<
                std::conjunction<
                    std::is_constructible<T, U&&>,
                    std::is_assignable<T&, U&&>,
                    std::disjunction<
                        std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, T>,
                        std::conjunction<
                            std::negation<std::is_convertible<U&&, Status>>,
                            std::negation<internal_statusor::HasConversionOperatorToStatusOr<T, U&&>>>>,
                    internal_statusor::IsForwardingAssignmentValid<T, U&&>>::value>::type>
#else
    template <typename U = T>
    requires requires {
        requires std::is_constructible_v<T, U&&>;
        requires std::is_assignable_v<T&, U&&>;
        requires internal_statusor::IsForwardingAssignmentValid<T, U&&>::value;
        requires std::is_same_v<std::remove_cv_t<std::remove_reference_t<U>>, T> ||
            (!std::is_convertible_v<U&&, Status> &&
            internal_statusor::HasConversionOperatorToStatusOr<T,U&&>::value);
    }
#endif
    StatusOr& operator=(U&& v) {
        this->Assign(std::forward<U>(v));
        return *this;
    }





    template <typename... Args>
    explicit StatusOr(std::in_place_t, Args&&... args)
        : Base(std::in_place, std::forward<Args>(args)...) {}

    template <typename U, typename... Args>
    explicit StatusOr(std::in_place_t, std::initializer_list<U> ilist, Args&&... args)
        : Base(std::in_place, ilist, std::forward<Args>(args)...) {}







    // Constructs the inner value `T` in-place using the provided args, using the
    // `T(U)` (direct-initialization) constructor. This constructor is only valid
    // if `T` can be constructed from a `U`. Can accept move or copy constructors.
    //
    // This constructor is explicit if `U` is not convertible to `T`. To avoid
    // ambiguity, this constructor is disabled if `U` is a `StatusOr<J>`, where
    // `J` is convertible to `T`.
#if __cplusplus <= 201703L
    template <
            typename U = T,
            std::enable_if_t<
                    std::conjunction<
                            internal_statusor::IsDirectInitializationValid<T, U&&>,
                            std::is_constructible<T, U&&>, std::is_convertible<U&&, T>,
                            std::disjunction<
                                    std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, T>,
                                    std::conjunction<
                                            std::negation<std::is_convertible<U&&, Status>>,
                                            std::negation< internal_statusor::HasConversionOperatorToStatusOr< T, U&&>>>>>::value,
                    int> = 0>
#else
    template < typename U = T>
    requires requires
    {
        requires internal_statusor::IsDirectInitializationValid<T, U&&>::value;
        requires std::is_constructible_v<T, U&&>;
        requires std::is_convertible_v<U&&, T>;
        requires std::is_same_v<std::remove_cv_t<std::remove_reference_t<U>>, T> ||
                    (!std::is_convertible_v<U&&, Status> && internal_statusor::HasConversionOperatorToStatusOr< T, U&&>::value);
    }
#endif
    StatusOr(U&& u) : StatusOr(std::in_place, std::forward<U>(u)) {} // NOLINT


#if __cplusplus <= 201703L
    template <
            typename U = T,
            std::enable_if_t<
                std::conjunction<
                    internal_statusor::IsDirectInitializationValid<T, U&&>,
                    std::disjunction<
                            std::is_same<std::remove_cv_t<std::remove_reference_t<U>>, T>,
                            std::conjunction<
                                    std::negation<std::is_constructible<Status, U&&>>,
                                    std::negation<internal_statusor::HasConversionOperatorToStatusOr< T, U&&>>>>,
                    std::is_constructible<T, U&&>,
                    std::negation<std::is_convertible<U&&, T>>>::value,
                int> = 0>
#else
    template <typename U = T>
    requires requires {
        requires  internal_statusor::IsDirectInitializationValid<T, U&&>::value;
        requires  std::is_constructible_v<T, U&&>;
        requires !std::is_convertible_v<U&&, T>;
        requires  std::is_same_v<std::remove_cv_t<std::remove_reference_t<U>>, T> ||
                    (!std::is_constructible_v<Status, U&&> && !internal_statusor::HasConversionOperatorToStatusOr< T, U&&>::value);
    }
#endif
    explicit StatusOr(U&& u) : StatusOr(std::in_place, std::forward<U>(u)) {} // NOLINT






    /// @brief 内部保存是异常状态，还是正常状态下的值
    [[nodiscard]] bool ok() const { return this->status_.ok(); }

    /// @brief 获取内部状态
    [[nodiscard]] const Status& status() const& { return this->status_; }
    [[nodiscard]]       Status  status() &&     { return ok() ? Status{} : std::move(this->status_); }

    //! 获取内部的值，若内部保存的是错误码，则抛出异常，因此使用前请使用保证ok()为true
    //针对const和移动的不同函数
    const T&  value() const&  { this->EnsureOk(); return this->data_; }
          T&  value()      &  { this->EnsureOk(); return this->data_; }
    const T&& value() const&& { this->EnsureOk(); return std::move(this->data_); }
          T&& value()      && { this->EnsureOk(); return std::move(this->data_); }

    //! 获取内部的值，若内部保存的是错误码，则抛出异常，因此使用前请使用保证ok()为true
    // 针对const和移动的不同函数
    const T&  operator*() const&  { this->EnsureOk(); return this->data_; }
          T&  operator*()      &  { this->EnsureOk(); return this->data_; }
    const T&& operator*() const&& { this->EnsureOk(); return std::move(this->data_); }
          T&& operator*()      && { this->EnsureOk(); return std::move(this->data_); }
    const T*  operator->() const  { this->EnsureOk(); return &this->data_; }
          T*  operator->()        { this->EnsureOk(); return &this->data_; }

    //! 尝试获取内部的值，若内部无值，不会抛出异常，而是按照给出default_value的构造类型U
    // 针对const和移动的不同函数
    template <typename U>
    T value_or(U&& default_value) const& { return ok() ? this->data_ : std::forward<U>(default_value); }
    template <typename U>
    T value_or(U&& default_value) && { return ok() ? std::move(this->data_) : std::forward<U>(default_value); }



    template <typename... Args>
    T& emplace(Args&&... args)
    {
        if (ok()) {
            this->Clear();
            this->MakeValue(std::forward<Args>(args)...);
        } else {
            this->MakeValue(std::forward<Args>(args)...);
            this->status_ = OkStatus();
        }
        return this->data_;
    }
#if __cplusplus <= 201703L
    template <typename U, typename... Args,
        std::enable_if_t<std::is_constructible_v<T, std::initializer_list<U>&, Args&&...>, int> = 0>
#else
    template <typename U, typename... Args>
    requires std::is_constructible_v<T, std::initializer_list<U>&, Args&&...>
#endif
    T& emplace(std::initializer_list<U> ilist, Args&&... args)
    {
        if (ok()) {
            this->Clear();
            this->MakeValue(ilist, std::forward<Args>(args)...);
        } else {
            this->MakeValue(ilist, std::forward<Args>(args)...);
            this->status_ = OkStatus();
        }
        return this->data_;
    }

private:
    using internal_statusor::StatusOrData<T>::Assign;

    template <typename U>
    void Assign(const StatusOr<U>& other)
    {
        if (other.ok()) {
            this->Assign(*other);
        } else {
            this->AssignStatus(other.status());
        }
    }

    template <typename U>
    void Assign(StatusOr<U>&& other)
    {
        if (other.ok()) {
            this->Assign(*std::move(other));
        } else {
            this->AssignStatus(std::move(other).status());
        }
    }
};




template <typename T>
bool operator==(const StatusOr<T>& lhs, const StatusOr<T>& rhs) {
    if (lhs.ok() && rhs.ok()) return *lhs == *rhs;
    return lhs.status() == rhs.status();
}

template <typename T>
bool operator!=(const StatusOr<T>& lhs, const StatusOr<T>& rhs) {
    return !(lhs == rhs);
}






}


#endif //____STATUSOR_HPP

#pragma clang diagnostic pop
