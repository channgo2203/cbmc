/*******************************************************************\

Module: C++ Language Type Checking

Author: Daniel Kroening, kroening@cs.cmu.edu

\*******************************************************************/

/// \file
/// C++ Language Type Checking

#include "cpp_typecheck.h"

#include <util/arith_tools.h>
#include <util/std_expr.h>

#include <linking/zero_initializer.h>
#include <util/c_types.h>
#include <ansi-c/c_sizeof.h>

#include "cpp_util.h"

/// Initialize an object with a value
void cpp_typecheckt::convert_initializer(symbolt &symbol)
{
  // this is needed for template arguments that are types

  if(symbol.is_type)
  {
    if(symbol.value.is_nil())
      return;

    if(symbol.value.id()!=ID_type)
    {
      error().source_location=symbol.location;
      error() << "expected type as initializer for `"
              << symbol.base_name << "'" << eom;
      throw 0;
    }

    typecheck_type(symbol.value.type());

    return;
  }

  // do we have an initializer?
  if(symbol.value.is_nil())
  {
    // do we need one?
    if(is_reference(symbol.type))
    {
      error().source_location=symbol.location;
      error() << "`" << symbol.base_name
              << "' is declared as reference but is not initialized"
              << eom;
      throw 0;
    }

    // done
    return;
  }

  // we do have an initializer

  if(is_reference(symbol.type))
  {
    typecheck_expr(symbol.value);
    reference_initializer(symbol.value, symbol.type);
  }
  else if(cpp_is_pod(symbol.type))
  {
    if(symbol.type.id() == ID_pointer &&
       symbol.type.subtype().id() == ID_code &&
       symbol.value.id() == ID_address_of &&
       symbol.value.op0().id() == ID_cpp_name)
    {
      // initialization of a function pointer with
      // the address of a function: use pointer type information
      // for the sake of overload resolution

      cpp_typecheck_fargst fargs;
      fargs.in_use = true;

      const code_typet &code_type=to_code_type(symbol.type.subtype());

      for(code_typet::parameterst::const_iterator
          ait=code_type.parameters().begin();
          ait!=code_type.parameters().end();
          ait++)
      {
        exprt new_object("new_object");
        new_object.set(ID_C_lvalue, true);
        new_object.type() = ait->type();

        if(ait->get(ID_C_base_name)==ID_this)
        {
          fargs.has_object = true;
          new_object.type() = ait->type().subtype();
        }

        fargs.operands.push_back(new_object);
      }

      exprt resolved_expr=resolve(
        to_cpp_name(static_cast<irept &>(symbol.value.op0())),
        cpp_typecheck_resolvet::wantt::BOTH, fargs);

      assert(symbol.type.subtype() == resolved_expr.type());

      if(resolved_expr.id()==ID_symbol)
      {
        symbol.value=
          address_of_exprt(resolved_expr);
      }
      else if(resolved_expr.id()==ID_member)
      {
        symbol.value =
          address_of_exprt(
            lookup(resolved_expr.get(ID_component_name)).symbol_expr());

        symbol.value.type().add("to-member") = resolved_expr.op0().type();
      }
      else
        assert(false);

      if(symbol.type != symbol.value.type())
      {
        error().source_location=symbol.location;
        error() << "conversion from `"
                << to_string(symbol.value.type()) << "' to `"
                << to_string(symbol.type) << "' " << eom;
        throw 0;
      }

      return;
    }

    typecheck_expr(symbol.value);

    if(symbol.value.id()==ID_initializer_list ||
       symbol.value.id()==ID_string_constant)
    {
      do_initializer(symbol.value, symbol.type, true);

      if(symbol.type.find(ID_size).is_nil())
        symbol.type=symbol.value.type();
    }
    else
      implicit_typecast(symbol.value, symbol.type);

    #if 0
    simplify_exprt simplify(*this);
    exprt tmp_value = symbol.value;
    if(!simplify.simplify(tmp_value))
      symbol.value.swap(tmp_value);
    #endif
  }
  else
  {
    // we need a constructor

    symbol_exprt expr_symbol(symbol.name, symbol.type);
    already_typechecked(expr_symbol);

    exprt::operandst ops;
    ops.push_back(symbol.value);

    symbol.value = cpp_constructor(
      symbol.value.source_location(),
      expr_symbol,
      ops);
  }
}

void cpp_typecheckt::zero_initializer(
  const exprt &object,
  const typet &type,
  const source_locationt &source_location,
  exprt::operandst &ops)
{
  const typet &final_type=follow(type);

  if(final_type.id()==ID_struct)
  {
    forall_irep(cit, final_type.find(ID_components).get_sub())
    {
      const exprt &component=static_cast<const exprt &>(*cit);

      if(component.type().id()==ID_code)
        continue;

      if(component.get_bool(ID_is_type))
        continue;

      if(component.get_bool(ID_is_static))
        continue;

      exprt member(ID_member);
      member.copy_to_operands(object);
      member.set(ID_component_name, component.get(ID_name));

      // recursive call
      zero_initializer(member, component.type(), source_location, ops);
    }
  }
  else if(final_type.id()==ID_array &&
          !cpp_is_pod(final_type.subtype()))
  {
    const array_typet &array_type=to_array_type(type);
    const exprt &size_expr=array_type.size();

    if(size_expr.id()==ID_infinity)
      return; // don't initialize

    mp_integer size;

    bool to_int=to_integer(size_expr, size);
    assert(!to_int);
    assert(size>=0);

    exprt::operandst empty_operands;
    for(mp_integer i=0; i<size; ++i)
    {
      exprt index(ID_index);
      index.copy_to_operands(object, from_integer(i, index_type()));
      zero_initializer(index, array_type.subtype(), source_location, ops);
    }
  }
  else if(final_type.id()==ID_union)
  {
    c_sizeoft c_sizeof(*this);

    // Select the largest component for zero-initialization
    mp_integer max_comp_size=0;

    exprt comp=nil_exprt();

    forall_irep(it, final_type.find(ID_components).get_sub())
    {
      const exprt &component=static_cast<const exprt &>(*it);

      assert(component.type().is_not_nil());

      if(component.type().id()==ID_code)
        continue;

      exprt component_size=c_sizeof(component.type());

      mp_integer size_int;
      if(!to_integer(component_size, size_int))
      {
        if(size_int>max_comp_size)
        {
          max_comp_size=size_int;
          comp=component;
        }
      }
    }

    if(max_comp_size>0)
    {
      irept name(ID_name);
      name.set(ID_identifier, comp.get(ID_base_name));
      name.set(ID_C_source_location, source_location);

      cpp_namet cpp_name;
      cpp_name.move_to_sub(name);

      exprt member(ID_member);
      member.copy_to_operands(object);
      member.set(ID_component_cpp_name, cpp_name);
      zero_initializer(member, comp.type(), source_location, ops);
    }
  }
  else if(final_type.id()==ID_c_enum)
  {
    typet enum_type(ID_unsignedbv);
    enum_type.add(ID_width)=final_type.find(ID_width);

    exprt zero(from_integer(0, enum_type));
    zero.make_typecast(type);
    already_typechecked(zero);

    code_assignt assign;
    assign.lhs()=object;
    assign.rhs()=zero;
    assign.add_source_location()=source_location;

    typecheck_expr(assign.lhs());
    assign.lhs().type().set(ID_C_constant, false);
    already_typechecked(assign.lhs());

    typecheck_code(assign);
    ops.push_back(assign);
  }
  else if(final_type.id()==ID_incomplete_struct ||
          final_type.id()==ID_incomplete_union)
  {
    error().source_location=source_location;
    error() << "cannot zero-initialize incomplete compound" << eom;
    throw 0;
  }
  else
  {
    exprt value=
      ::zero_initializer(
        final_type, source_location, *this, get_message_handler());

    code_assignt assign;
    assign.lhs()=object;
    assign.rhs()=value;
    assign.add_source_location()=source_location;

    typecheck_expr(assign.op0());
    assign.lhs().type().set(ID_C_constant, false);
    already_typechecked(assign.lhs());

    typecheck_code(assign);
    ops.push_back(assign);
  }
}
