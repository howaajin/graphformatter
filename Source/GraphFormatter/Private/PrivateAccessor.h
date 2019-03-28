/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

template<typename TClass>
struct FPrivateAccessor
{
	static typename TClass::MemberType Member;
};

template<typename TClass>
typename TClass::MemberType FPrivateAccessor<TClass>::Member;

template<typename TClass, typename TClass::MemberType m>
struct FPrivateRob : FPrivateAccessor<TClass>
{
	struct Setter
	{
		Setter() { FPrivateAccessor<TClass>::Member = m; }
	};
	static Setter Instance;
};

template<class TClass, class TType, typename... Args>
struct FConstFunctionWrapper
{
	using Signature = TType(TClass::*)(Args...) const;
};

template<class TClass, class TType, typename... Args>
struct FunctionWrapper
{
	using Signature = TType(TClass::*)(Args...);
};

template<typename TClass, typename TClass::MemberType m>
typename FPrivateRob<TClass, m>::Setter FPrivateRob<TClass, m>::Instance;

#define DECLARE_PRIVATE_MEMBER_ACCESSOR( Name, TargetClass, TargetMemberType, TargetMember ) \
struct Name \
{ \
	typedef TargetMemberType TargetClass::*MemberType; \
}; \
template struct FPrivateRob<Name, &TargetClass::TargetMember>;

#define DECLARE_PRIVATE_CONST_FUNC_ACCESSOR( Name, TargetClass, TargetMember, ReturnType, ... ) \
struct Name \
{ \
	using MemberType = FConstFunctionWrapper<TargetClass, ReturnType, __VA_ARGS__>::Signature; \
}; \
template struct FPrivateRob<Name, &TargetClass::TargetMember>;

#define DECLARE_PRIVATE_FUNC_ACCESSOR( Name, TargetClass, TargetMember, ReturnType, ... )\
struct Name \
{ \
	using MemberType = FunctionWrapper<TargetClass, ReturnType, __VA_ARGS__>::Signature; \
}; \
template struct FPrivateRob<Name, &TargetClass::TargetMember>;
