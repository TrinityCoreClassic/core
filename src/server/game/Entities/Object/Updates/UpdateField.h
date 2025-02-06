/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UpdateField_h__
#define UpdateField_h__

#include "ObjectGuid.h"
#include "Optional.h"
#include "UpdateMask.h"
#include <algorithm>
#include <memory>
#include <vector>

class ByteBuffer;
class Object;

namespace UF
{
    enum class UpdateFieldFlag : uint8
    {
        None = 0,
        Owner = 0x01,
        PartyMember = 0x02,
        UnitAll = 0x04,
        Empath = 0x08
    };

    DEFINE_ENUM_FLAG(UpdateFieldFlag);

    template<typename T>
    class UpdateFieldBase;

    template<typename T, uint32 BlockBit, uint32 Bit>
    class UpdateField;

    template<typename T>
    class UpdateFieldArrayBaseWithoutSize;

    template<typename T, std::size_t Size>
    class UpdateFieldArrayBase;

    template<typename T, std::size_t Size, uint32 Bit, uint32 FirstElementBit>
    class UpdateFieldArray;

    template<typename T>
    class DynamicUpdateFieldBase;

    template<typename T, uint32 BlockBit, uint32 Bit>
    class DynamicUpdateField;

    template<typename T>
    class OptionalUpdateFieldBase;

    template<typename T, uint32 BlockBit, uint32 Bit>
    class OptionalUpdateField;

    template<typename T, bool PublicSet>
    struct MutableFieldReference;

    template<typename T, bool PublicSet>
    struct MutableNestedFieldReference;

    struct IsUpdateFieldStructureTag
    {
    };
    struct HasChangesMaskTag
    {
    };
    struct IsUpdateFieldHolderTag
    {
    };

    namespace Compat
    {
        enum class UpdateFieldFlag : uint32
        {
            None = 0x000,
            Public = 0x001,
            Private = 0x002,
            Owner = 0x004,
            ItemOwner = 0x008,
            SpecialInfo = 0x010,
            PartyMember = 0x020,
            UnitAll = 0x040,
            Dynamic = 0x080,
            Flag0x100 = 0x100,
            Urgent = 0x200,
            UrgentSelfOnly = 0x400

        };

        DEFINE_ENUM_FLAG(UpdateFieldFlag);

        struct UpdateFlags
        {
            UpdateFieldFlag visibilityFlags;
            UpdateFieldFlag notifyFlags;
        };

        struct HasDynamicChangesMaskTag
        {
        };

        template<std::size_t Bits>
        struct HasDynamicChangesMask;

        template<typename T, uint32 Index, uint32 Offset>
        class UpdateField;

        template<typename T, std::size_t Size, uint32 Index, uint32 Offset>
        class UpdateFieldArray;

        template<typename T, uint32 Index, uint32 Offset>
        class DynamicUpdateField;

        template<typename T, uint32 Index, uint32 Offset>
        class OptionalUpdateField;

    }

    template<typename T>
    struct UpdateFieldSetter
    {
        using value_type = T;

        template<typename F>
        friend bool SetUpdateFieldValue(UpdateFieldSetter<F>& setter, typename UpdateFieldSetter<F>::value_type&& value);

        UpdateFieldSetter(T& value) : _value(value)
        {
        }

        T GetValue() const
        {
            return _value;
        }

    private:
        bool SetValue(T value)
        {
            if (_value != value)
            {
                _value = std::move(value);
                return true;
            }
            return false;
        }

        T& _value;
    };

    // Same as UpdateFieldSetter but with public setter, used to set member fields for values added to dynamic fields
    template<typename T>
    struct UpdateFieldPublicSetter
    {
        using value_type = T;

        UpdateFieldPublicSetter(T& value) : _value(value)
        {
        }

        T GetValue() const
        {
            return _value;
        }

        void SetValue(T value)
        {
            _value = std::move(value);
        }

    private:
        T& _value;
    };

    template<typename T>
    struct DynamicUpdateFieldSetter
    {
        using value_type = T;
        using insert_result = std::conditional_t<std::is_base_of<HasChangesMaskTag, T>::value, MutableFieldReference<T, true>, T&>;

        template<typename F>
        friend typename DynamicUpdateFieldSetter<F>::insert_result AddDynamicUpdateFieldValue(DynamicUpdateFieldSetter<F>& setter);

        template<typename F>
        friend typename DynamicUpdateFieldSetter<F>::insert_result InsertDynamicUpdateFieldValue(DynamicUpdateFieldSetter<F>& setter, uint32 index);

        template<typename F>
        friend void RemoveDynamicUpdateFieldValue(DynamicUpdateFieldSetter<F>& setter, uint32 index);

        template<typename F>
        friend void ClearDynamicUpdateFieldValues(DynamicUpdateFieldSetter<F>& setter);

        DynamicUpdateFieldSetter(std::vector<T>& values, std::vector<uint32>& updateMask, Compat::DynamicFieldChangeType& changeType) : _values(values), _updateMask(updateMask), _changeType(changeType)
        {
        }

    private:
        insert_result AddValue()
        {
            MarkChanged(_values.size());
            _values.emplace_back();
            T& value = _values.back();
            MarkNewValue(value, std::is_base_of<HasChangesMaskTag, T>{});
            _changeType = Compat::DynamicFieldChangeType::VALUE_AND_SIZE_CHANGED;
            return { value };
        }

        insert_result InsertValue(std::size_t index)
        {
            _values.emplace(_values.begin() + index);
            for (std::size_t i = index; i < _values.size(); ++i)
            {
                MarkChanged(i);
                // also mark all fields of value as changed
                MarkNewValue(_values[i], std::is_base_of<HasChangesMaskTag, T>{});
            }
            _changeType = Compat::DynamicFieldChangeType::VALUE_AND_SIZE_CHANGED;
            return { _values[index] };
        }

        void RemoveValue(std::size_t index)
        {
            // remove by shifting entire container - client might rely on values being sorted for certain fields
            _values.erase(_values.begin() + index);
            for (std::size_t i = index; i < _values.size(); ++i)
            {
                MarkChanged(i);
                // also mark all fields of value as changed
                MarkNewValue(_values[i], std::is_base_of<HasChangesMaskTag, T>{});
            }
            if (_values.size() % 32)
                _updateMask[UpdateMaskHelpers::GetBlockIndex(_values.size())] &= ~UpdateMaskHelpers::GetBlockFlag(_values.size());
            else
                _updateMask.pop_back();

            if (_changeType == Compat::DynamicFieldChangeType::UNCHANGED) {
                _changeType = Compat::DynamicFieldChangeType::VALUE_CHANGED;
            }
        }

        void Clear()
        {
            _values.clear();
            _updateMask.clear();
            _changeType = Compat::DynamicFieldChangeType::VALUE_AND_SIZE_CHANGED;
        }

        void MarkChanged(std::size_t index)
        {
            std::size_t block = UpdateMaskHelpers::GetBlockIndex(index);
            if (block >= _updateMask.size())
                _updateMask.resize(block + 1);

            _updateMask[block] |= UpdateMaskHelpers::GetBlockFlag(index);
        }

        void MarkNewValue(T&, std::false_type)
        {
        }

        void MarkNewValue(T& value, std::true_type)
        {
            value._changesMask.SetAll();
        }

        std::vector<T>& _values;
        std::vector<uint32>& _updateMask;
        Compat::DynamicFieldChangeType& _changeType;
    };

    template<typename T>
    struct OptionalUpdateFieldSetter
    {
        template<typename F>
        friend void RemoveOptionalUpdateFieldValue(OptionalUpdateFieldSetter<F>& setter);

        OptionalUpdateFieldSetter(OptionalUpdateFieldBase<T>& field) : _field(field) { }

    private:
        void RemoveValue()
        {
            if (_field.has_value())
                _field.DestroyValue();
        }

        OptionalUpdateFieldBase<T>& _field;
    };

    template<typename T, bool PublicSet>
    struct MutableFieldReference
    {
        MutableFieldReference(T& value) : _value(value)
        {
        }

        template<typename V, uint32 BlockBit, uint32 Bit, typename U = T>
        std::enable_if_t<std::is_base_of<HasChangesMaskTag, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, V>::value,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, V>::value,
            MutableNestedFieldReference<V, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<V>, UpdateFieldSetter<V>>>>>
            ModifyValue(UpdateField<V, BlockBit, Bit>(T::* field))
        {
            _value._changesMask.Set(BlockBit);
            _value._changesMask.Set(Bit);
            return { (_value.*field)._value };
        }

        template<typename V, std::size_t Size, uint32 Bit, uint32 FirstElementBit, typename U = T>
        std::enable_if_t<std::is_base_of<HasChangesMaskTag, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, V>::value,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, V>::value,
            MutableNestedFieldReference<V, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<V>, UpdateFieldSetter<V>>>>>
            ModifyValue(UpdateFieldArray<V, Size, Bit, FirstElementBit>(T::* field), uint32 index)
        {
            _value._changesMask.Set(Bit);
            _value._changesMask.Set(FirstElementBit + index);
            return { (_value.*field)._values[index] };
        }

        template<typename V, uint32 BlockBit, uint32 Bit, typename U = T>
        std::enable_if_t<std::is_base_of<HasChangesMaskTag, U>::value, DynamicUpdateFieldSetter<V>>
            ModifyValue(DynamicUpdateField<V, BlockBit, Bit>(T::* field))
        {
            _value._changesMask.Set(BlockBit);
            _value._changesMask.Set(Bit);
            return { (_value.*field)._values, (_value.*field)._updateMask, (_value.*field)._changeType };
        }

        template<typename V, uint32 BlockBit, uint32 Bit, typename U = T>
        std::enable_if_t<std::is_base_of<HasChangesMaskTag, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, V>::value,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, V>::value,
            MutableNestedFieldReference<V, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<V>, UpdateFieldSetter<V>>>>>
            ModifyValue(DynamicUpdateField<V, BlockBit, Bit>(T::* field), uint32 index)
        {
            if (index >= (_value.*field).size())
            {
                // fill with zeros until reaching desired slot
                (_value.*field)._values.resize(index + 1);
                (_value.*field)._updateMask.resize(((_value.*field)._values.size() + 31) / 32);
            }

            _value._changesMask.Set(BlockBit);
            _value._changesMask.Set(Bit);
            (_value.*field).MarkChanged(index);
            return { (_value.*field)._values[index] };
        }

        template<typename V, uint32 BlockBit, uint32 Bit, typename U = T>
        std::enable_if_t<std::is_base_of<HasChangesMaskTag, U>::value, OptionalUpdateFieldSetter<V>>
            ModifyValue(OptionalUpdateField<V, BlockBit, Bit>(T::* field))
        {
            _value._changesMask.Set(BlockBit);
            _value._changesMask.Set(Bit);
            return { _value.*field };
        }

        template<typename V, uint32 BlockBit, uint32 Bit, typename U = T>
        std::enable_if_t<std::is_base_of<HasChangesMaskTag, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, V>::value,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, V>::value,
            MutableNestedFieldReference<V, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<V>, UpdateFieldSetter<V>>>>>
            ModifyValue(OptionalUpdateField<V, BlockBit, Bit>(T::* field), uint32 /*dummy*/)
        {
            if (!(_value.*field).has_value())
                (_value.*field).ConstructValue();

            _value._changesMask.Set(BlockBit);
            _value._changesMask.Set(Bit);
            return { *((_value.*field)._value) };
        }

        template<typename V, typename U = T>
        std::enable_if_t<!std::is_base_of<HasChangesMaskTag, U>::value && !std::is_array<V>::value,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<V>, UpdateFieldSetter<V>>>
            ModifyValue(V(T::* field))
        {
            return { _value.*field };
        }

        template<typename V, std::size_t Size, typename U = T>
        std::enable_if_t<!std::is_base_of<HasChangesMaskTag, U>::value,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<V>, UpdateFieldSetter<V>>>
            ModifyValue(V(T::* field)[Size], uint32 index)
        {
            return { (_value.*field)[index] };
        }


        // Compat handling.

        template<typename V, uint32 Index, uint32 Offset, typename U = T>
        std::enable_if_t<std::is_base_of<HasChangesMaskTag, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, V>::value,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, V>::value,
            MutableNestedFieldReference<V, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<V>, UpdateFieldSetter<V>>>>>
            ModifyValue(Compat::UpdateField<V, Index, Offset>(T::* field))
        {
            constexpr auto size = sizeof(typename Compat::UpdateField<V, Index, Offset>::value_type);
            constexpr auto blocks = Compat::GetBlockCount(size * 8);
            constexpr bool has_mask = std::is_base_of<HasChangesMaskTag, V>::value;

            if constexpr (!has_mask)
            {
                for (auto i = 0; i < blocks; i++)
                {
                    _value._changesMask.Set(Index + i);
                }
            }
                
            return { (_value.*field)._value };
        }

        template<typename V, std::size_t Size, uint32 Index, uint32 Offset, typename U = T>
        std::enable_if_t<std::is_base_of<HasChangesMaskTag, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, V>::value,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, V>::value,
            MutableNestedFieldReference<V, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<V>, UpdateFieldSetter<V>>>>>
            ModifyValue(Compat::UpdateFieldArray<V, Size, Index, Offset>(T::* field), uint32 index)
        {
            constexpr auto size = sizeof(typename Compat::UpdateFieldArray<V, Size, Index, Offset>::value_type);
            constexpr auto block_size = sizeof(Compat::BlockType);
            constexpr auto blocks = Compat::GetBlockCount(size * 8);
            constexpr bool has_mask = std::is_base_of<HasChangesMaskTag, V>::value;

            if constexpr (!has_mask)
            {
                static_assert((size % block_size) == 0 || (block_size % size) == 0);
                constexpr auto values_per_block = size <= block_size ? block_size / size : 1;
                constexpr auto blocks_per_value = size > block_size ? size / block_size : 1;
                const auto block_start_index = size <= block_size ? (index / values_per_block) : (index * blocks_per_value);

                for (auto i = 0; i < blocks; i++)
                {
                    _value._changesMask.Set(Index + block_start_index + i);
                }
            }

            return { (_value.*field)._values[index] };
        }

        template<typename V, uint32 Index, uint32 Offset, typename U = T>
        std::enable_if_t<std::is_base_of<Compat::HasDynamicChangesMaskTag, U>::value, DynamicUpdateFieldSetter<V>>
            ModifyValue(Compat::DynamicUpdateField<V, Index, Offset>(T::* field))
        {
            _value._changesDynamicMask.Set(Index);
            return { (_value.*field)._values, (_value.*field)._updateMask, (_value.*field)._changeType };
        }

        template<typename V, uint32 Index, uint32 Offset, typename U = T>
        std::enable_if_t<std::is_base_of<Compat::HasDynamicChangesMaskTag, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, V>::value,
            MutableFieldReference<V, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, V>::value,
            MutableNestedFieldReference<V, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<V>, UpdateFieldSetter<V>>>>>
            ModifyValue(Compat::DynamicUpdateField<V, Index, Offset>(T::* field), uint32 index)
        {
            if (index >= (_value.*field).size())
            {
                // fill with zeros until reaching desired slot
                (_value.*field)._values.resize(index + 1);
                (_value.*field)._updateMask.resize(((_value.*field)._values.size() + 31) / 32);
            }

            _value._changesDynamicMask.Set(Index);
            (_value.*field).MarkChanged(index);
            return { (_value.*field)._values[index] };
        }

    private:
        T& _value;
    };

    template<typename T, bool PublicSet>
    struct MutableNestedFieldReference
    {
        using value_type = typename T::value_type;

        MutableNestedFieldReference(T& value) : _value(value)
        {
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of<UpdateFieldBase<value_type>, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, value_type>::value,
            MutableFieldReference<value_type, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, value_type>::value,
            MutableNestedFieldReference<value_type, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<value_type>, UpdateFieldSetter<value_type>>>>>
            ModifyValue()
        {
            return { _value._value };
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of<UpdateFieldArrayBaseWithoutSize<value_type>, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, value_type>::value,
            MutableFieldReference<value_type, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, value_type>::value,
            MutableNestedFieldReference<value_type, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<value_type>, UpdateFieldSetter<value_type>>>>>
            ModifyValue(uint32 index)
        {
            return { _value._values[index] };
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of<DynamicUpdateFieldBase<value_type>, U>::value, DynamicUpdateFieldSetter<value_type>>
            ModifyValue()
        {
            return { _value._values, _value._updateMask };
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of<DynamicUpdateFieldBase<value_type>, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, value_type>::value,
            MutableFieldReference<value_type, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, value_type>::value,
            MutableNestedFieldReference<value_type, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<value_type>, UpdateFieldSetter<value_type>>>>>
            ModifyValue(uint32 index)
        {
            if (index >= _value.size())
            {
                // fill with zeros until reaching desired slot
                _value._values.resize(index + 1);
                _value._updateMask.resize((_value._values.size() + 31) / 32);
            }

            _value.MarkChanged(index);
            return { _value._values[index] };
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of<OptionalUpdateFieldBase<value_type>, U>::value, OptionalUpdateFieldSetter<value_type>>
            ModifyValue()
        {
            return { _value };
        }

        template<typename U = T>
        std::enable_if_t<std::is_base_of<OptionalUpdateFieldBase<value_type>, U>::value,
            std::conditional_t<std::is_base_of<IsUpdateFieldStructureTag, value_type>::value,
            MutableFieldReference<value_type, PublicSet>,
            std::conditional_t<std::is_base_of<IsUpdateFieldHolderTag, value_type>::value,
            MutableNestedFieldReference<value_type, PublicSet>,
            std::conditional_t<PublicSet, UpdateFieldPublicSetter<value_type>, UpdateFieldSetter<value_type>>>>>
            ModifyValue(uint32 /*dummy*/)
        {
            if (!_value.has_value())
                _value.ConstructValue();

            return { *(_value._value) };
        }

    private:
        T& _value;
    };

    template<std::size_t Bits>
    class HasChangesMask : public HasChangesMaskTag
    {
        template<typename T>
        friend struct DynamicUpdateFieldSetter;

        template<typename T, bool PublicSet>
        friend struct MutableFieldReference;

        template<typename T, uint32 BlockBit, uint32 Bit>
        friend class UpdateField;

        template<typename T, std::size_t Size, uint32 Bit, uint32 FirstElementBit>
        friend class UpdateFieldArray;

        template<typename T, uint32 BlockBit, uint32 Bit>
        friend class DynamicUpdateField;

    public:
        using Base = HasChangesMask<Bits>;
        using Mask = UpdateMask<Bits>;

        //TODOFROST - confirm this works with compat - maybe need specific overloads?

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        MutableFieldReference<T, false> ModifyValue(UpdateField<T, BlockBit, Bit>(Derived::* field))
        {
            MarkChanged(field);
            return { (static_cast<Derived*>(this)->*field)._value };
        }

        template<typename Derived, typename T, std::size_t Size, uint32 Bit, uint32 FirstElementBit>
        MutableFieldReference<T, false> ModifyValue(UpdateFieldArray<T, Size, Bit, FirstElementBit>(Derived::* field), uint32 index)
        {
            MarkChanged(field, index);
            return { (static_cast<Derived*>(this)->*field)._values[index] };
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        MutableFieldReference<T, false> ModifyValue(DynamicUpdateField<T, BlockBit, Bit>(Derived::* field))
        {
            MarkChanged(field);
            return { (static_cast<Derived*>(this)->*field)._values };
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        MutableFieldReference<T, false> ModifyValue(DynamicUpdateField<T, BlockBit, Bit>(Derived::* field), uint32 index)
        {
            DynamicUpdateField<T, BlockBit, Bit>& uf = (static_cast<Derived*>(this)->*field);
            if (index >= uf.size())
            {
                // fill with zeros until reaching desired slot
                uf._values.resize(index + 1);
                uf._updateMask.resize((uf._values.size() + 31) / 32);
            }

            MarkChanged(field);
            (static_cast<Derived*>(this)->*field).MarkChanged(index);
            return { uf._values[index] };
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        MutableFieldReference<T, false> ModifyValue(OptionalUpdateField<T, BlockBit, Bit>(Derived::* field))
        {
            MarkChanged(field);
            return { *((static_cast<Derived*>(this)->*field)._value) };
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        void MarkChanged(UpdateField<T, BlockBit, Bit>(Derived::*))
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Set(BlockBit);
            _changesMask.Set(Bit);
        }

        template<typename Derived, typename T, std::size_t Size, uint32 Bit, uint32 FirstElementBit>
        void MarkChanged(UpdateFieldArray<T, Size, Bit, FirstElementBit>(Derived::*), uint32 index)
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Set(Bit);
            _changesMask.Set(FirstElementBit + index);
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        void MarkChanged(DynamicUpdateField<T, BlockBit, Bit>(Derived::*), uint32)
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Set(BlockBit);
            _changesMask.Set(Bit);
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        void MarkChanged(OptionalUpdateField<T, BlockBit, Bit>(Derived::*))
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Set(BlockBit);
            _changesMask.Set(Bit);
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        void ClearChanged(UpdateField<T, BlockBit, Bit>(Derived::*))
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Reset(Bit);
        }

        template<typename Derived, typename T, std::size_t Size, uint32 Bit, uint32 FirstElementBit>
        void ClearChanged(UpdateFieldArray<T, Size, Bit, FirstElementBit>(Derived::*), uint32 index)
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Reset(FirstElementBit + index);
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        void ClearChanged(DynamicUpdateField<T, BlockBit, Bit>(Derived::* field), uint32 index)
        {
            _changesMask.Reset(Bit);
            (static_cast<Derived*>(this)->*field).ClearChanged(index);
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        void ClearChanged(OptionalUpdateField<T, BlockBit, Bit>(Derived::*))
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Reset(Bit);
        }

        // compat

        template<typename Derived, typename T, uint32 Index, uint32 Offset>
        void MarkChanged(Compat::UpdateField<T, Index, Offset>(Derived::*))
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Set(Index);
        }

        template<typename Derived, typename T, std::size_t Size, uint32 Index, uint32 Offset>
        void MarkChanged(Compat::UpdateFieldArray<T, Size, Index, Offset>(Derived::*), uint32 index)
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Set(Index + index);
        }

        template<typename Derived, typename T, uint32 Index, uint32 Offset>
        void ClearChanged(Compat::UpdateField<T, Index, Offset>(Derived::*))
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Reset(Index);
        }

        template<typename Derived, typename T, std::size_t Size, uint32 Index, uint32 Offset>
        void ClearChanged(Compat::UpdateFieldArray<T, Size, Index, Offset>(Derived::*), uint32 index)
        {
            static_assert(std::is_base_of<Base, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

            _changesMask.Reset(Index + index);
        }

        Mask const& GetChangesMask() const { return _changesMask; }

    protected:
        template<typename T, uint32 BlockBit, uint32 Bit>
        void ClearChangesMask(UpdateField<T, BlockBit, Bit>& field)
        {
            ClearChangesMask(field, std::is_base_of<HasChangesMaskTag, T>{});
        }

        template<typename T, uint32 BlockBit, uint32 Bit>
        void ClearChangesMask(UpdateField<T, BlockBit, Bit>&, std::false_type) { }

        template<typename T, uint32 BlockBit, uint32 Bit>
        void ClearChangesMask(UpdateField<T, BlockBit, Bit>& field, std::true_type)
        {
            field._value.ClearChangesMask();
        }

        template<typename T, std::size_t Size, uint32 Bit, uint32 FirstElementBit>
        void ClearChangesMask(UpdateFieldArray<T, Size, Bit, FirstElementBit>& field)
        {
            ClearChangesMask(field, std::is_base_of<HasChangesMaskTag, T>{});
        }

        template<typename T, std::size_t Size, uint32 Bit, uint32 FirstElementBit>
        void ClearChangesMask(UpdateFieldArray<T, Size, Bit, FirstElementBit>&, std::false_type) { }

        template<typename T, std::size_t Size, uint32 Bit, uint32 FirstElementBit>
        void ClearChangesMask(UpdateFieldArray<T, Size, Bit, FirstElementBit>& field, std::true_type)
        {
            for (uint32 i = 0; i < Size; ++i)
                field._values[i].ClearChangesMask();
        }

        template<typename T, uint32 BlockBit, uint32 Bit>
        void ClearChangesMask(DynamicUpdateField<T, BlockBit, Bit>& field)
        {
            ClearChangesMask(field, std::is_base_of<HasChangesMaskTag, T>{});
            field.ClearChangesMask();
        }

        template<typename T, uint32 BlockBit, uint32 Bit>
        void ClearChangesMask(DynamicUpdateField<T, BlockBit, Bit>&, std::false_type) { }

        template<typename T, uint32 BlockBit, uint32 Bit>
        void ClearChangesMask(DynamicUpdateField<T, BlockBit, Bit>& field, std::true_type)
        {
            for (uint32 i = 0; i < field._values.size(); ++i)
                field._values[i].ClearChangesMask();
        }

        template<typename T, uint32 BlockBit, uint32 Bit>
        void ClearChangesMask(OptionalUpdateField<T, BlockBit, Bit>& field)
        {
            ClearChangesMask(field, std::is_base_of<HasChangesMaskTag, T>{});
        }

        template<typename T, uint32 BlockBit, uint32 Bit>
        void ClearChangesMask(OptionalUpdateField<T, BlockBit, Bit>&, std::false_type) { }

        template<typename T, uint32 BlockBit, uint32 Bit>
        void ClearChangesMask(OptionalUpdateField<T, BlockBit, Bit>& field, std::true_type)
        {
            if (field.has_value())
                field._value->ClearChangesMask();
        }

        //compat

        template<typename T, uint32 Index, uint32 Offset>
        void ClearChangesMask(Compat::UpdateField<T, Index, Offset>& field)
        {
            ClearChangesMask(field, std::is_base_of<HasChangesMaskTag, T>{});
        }

        template<typename T, uint32 Index, uint32 Offset>
        void ClearChangesMask(Compat::UpdateField<T, Index, Offset>&, std::false_type) { }

        template<typename T, uint32 Index, uint32 Offset>
        void ClearChangesMask(Compat::UpdateField<T, Index, Offset>& field, std::true_type)
        {
            field._value.ClearChangesMask();
        }

        template<typename T, std::size_t Size, uint32 Index, uint32 Offset>
        void ClearChangesMask(Compat::UpdateFieldArray<T, Size, Index, Offset>& field)
        {
            ClearChangesMask(field, std::is_base_of<HasChangesMaskTag, T>{});
        }

        template<typename T, std::size_t Size, uint32 Index, uint32 Offset>
        void ClearChangesMask(Compat::UpdateFieldArray<T, Size, Index, Offset>&, std::false_type) { }

        template<typename T, std::size_t Size, uint32 Index, uint32 Offset>
        void ClearChangesMask(Compat::UpdateFieldArray<T, Size, Index, Offset>& field, std::true_type)
        {
            for (uint32 i = 0; i < Size; ++i)
                field._values[i].ClearChangesMask();
        }

        Mask _changesMask;
    };

    class UpdateFieldHolder
    {
    public:
        explicit UpdateFieldHolder(Object* owner) : _owner(owner)
        {
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        MutableFieldReference<T, false> ModifyValue(UpdateField<T, BlockBit, Bit>(Derived::* field))
        {
            _changesMask.Set(Bit);
            return { (static_cast<Derived*>(_owner)->*field)._value };
        }

        template<typename Derived, typename T, uint32 BlockBit, uint32 Bit>
        void ClearChangesMask(UpdateField<T, BlockBit, Bit>(Derived::* field))
        {
            _changesMask.Reset(Bit);
            (static_cast<Derived*>(_owner)->*field)._value.ClearChangesMask();
        }

        template<typename Derived, typename T, uint32 Index, uint32 Offset>
                MutableFieldReference<T, false> ModifyValue(Compat::UpdateField<T, Index, Offset>(Derived::* field))
        {
            // compat no-op
            return { (static_cast<Derived*>(_owner)->*field)._value };
        }

        template<typename Derived, typename T, uint32 Index, uint32 Offset>
        void ClearChangesMask(Compat::UpdateField<T, Index, Offset>(Derived::* field))
        {
            // compat no-op
            (static_cast<Derived*>(_owner)->*field)._value.ClearChangesMask();
        }

        uint32 GetChangedObjectTypeMask() const
        {
            return _changesMask.GetBlock(0);
        }

        bool HasChanged(uint32 index) const
        {
            return _changesMask[index];
        }

    private:
        UpdateMask<NUM_CLIENT_OBJECT_TYPES> _changesMask;
        Object* _owner;
    };

    template<typename T>
    class UpdateFieldBase : public IsUpdateFieldHolderTag
    {
        template<typename F, bool PublicSet>
        friend struct MutableFieldReference;

        template<typename F, bool PublicSet>
        friend struct MutableNestedFieldReference;

        template<std::size_t Bits>
        friend class HasChangesMask;

        friend class UpdateFieldHolder;

    public:
        using value_type = T;

        operator T const& () const
        {
            return _value;
        }
        T const* operator->() const
        {
            return &_value;
        }
        T const& operator*() const
        {
            return _value;
        }

    private:
        T _value = {};
    };

    template<typename T, uint32 BlockBit, uint32 Bit>
    class UpdateField : public UpdateFieldBase<T>
    {
    };

    template<typename T>
    class UpdateFieldArrayBaseWithoutSize : public IsUpdateFieldHolderTag
    {
    };

    template<typename T, std::size_t Size>
    class UpdateFieldArrayBase : public UpdateFieldArrayBaseWithoutSize<T>
    {
        template<typename F, bool PublicSet>
        friend struct MutableFieldReference;

        template<typename F, bool PublicSet>
        friend struct MutableNestedFieldReference;

        template<std::size_t Bits>
        friend class HasChangesMask;

    public:
        using value_type = T;

        T const* begin() const
        {
            return std::begin(_values);
        }

        T const* end() const
        {
            return std::end(_values);
        }

        T const* data() const
        {
            return _values;
        }

        static constexpr std::size_t size()
        {
            return Size;
        }

        T const& operator[](std::size_t index) const
        {
            return _values[index];
        }

        int32 FindIndex(T const& value) const
        {
            auto itr = std::find(begin(), end(), value);
            if (itr != end())
                return int32(std::distance(begin(), itr));

            return -1;
        }

        template<typename Pred>
        int32 FindIndexIf(Pred pred) const
        {
            auto itr = std::find_if(begin(), end(), std::ref(pred));
            if (itr != end())
                return int32(std::distance(begin(), itr));

            return -1;
        }

    private:
        T _values[Size] = {};
    };

    // workaround functions for internal compiler errors in msvc 19.33.31629
    template<typename T>
    constexpr std::size_t size()
    {
        return T::size();
    }

    template<typename T>
    constexpr std::size_t size_of_value_type()
    {
        return sizeof(typename T::value_type);
    }

    template<typename T, std::size_t Size, uint32 Bit, uint32 FirstElementBit>
    class UpdateFieldArray : public UpdateFieldArrayBase<T, Size>
    {
    };

    void WriteDynamicFieldUpdateMask(std::size_t size, std::vector<uint32> const& updateMask, ByteBuffer& data, int32 bitsForSize = 32);
    void WriteCompleteDynamicFieldUpdateMask(std::size_t size, ByteBuffer& data, int32 bitsForSize = 32);

    template<typename T>
    class DynamicUpdateFieldBase : public IsUpdateFieldHolderTag
    {
        template<typename F, bool PublicSet>
        friend struct MutableFieldReference;

        template<typename F, bool PublicSet>
        friend struct MutableNestedFieldReference;

        template<std::size_t Bits>
        friend class HasChangesMask;

    public:
        using value_type = T;

        T const* data() const
        {
            return _values.data();
        }

        typename std::vector<T>::const_iterator begin() const
        {
            return _values.begin();
        }

        typename std::vector<T>::const_iterator end() const
        {
            return _values.end();
        }

        bool empty() const
        {
            return _values.empty();
        }

        std::size_t size() const
        {
            return _values.size();
        }

        T const& operator[](std::size_t index) const
        {
            return _values[index];
        }

        int32 FindIndex(T const& value) const
        {
            auto itr = std::find(_values.begin(), _values.end(), value);
            if (itr != _values.end())
                return int32(std::distance(_values.begin(), itr));

            return -1;
        }

        template<typename Pred>
        int32 FindIndexIf(Pred pred) const
        {
            auto itr = std::find_if(_values.begin(), _values.end(), std::ref(pred));
            if (itr != _values.end())
                return int32(std::distance(_values.begin(), itr));

            return -1;
        }

        bool HasChanged(uint32 index) const
        {
            return (_updateMask[index / 32] & (1 << (index % 32))) != 0;
        }

        void WriteUpdateMask(ByteBuffer& data, int32 bitsForSize = 32) const
        {
            WriteDynamicFieldUpdateMask(_values.size(), _updateMask, data, bitsForSize);
        }

        Compat::DynamicFieldChangeType ChangeType() const
        {
            return _changeType;
        }

    private:
        void MarkChanged(std::size_t index)
        {
            std::size_t block = UpdateMaskHelpers::GetBlockIndex(index);
            if (block >= _updateMask.size())
                _updateMask.emplace_back(0);

            _updateMask[block] |= UpdateMaskHelpers::GetBlockFlag(index);
            if (_changeType == Compat::DynamicFieldChangeType::UNCHANGED) {
                _changeType = Compat::DynamicFieldChangeType::VALUE_CHANGED;
            }
        }

        void ClearChanged(std::size_t index)
        {
            std::size_t block = UpdateMaskHelpers::GetBlockIndex(index);
            if (block >= _updateMask.size())
                _updateMask.emplace_back(0);

            _updateMask[block] &= ~UpdateMaskHelpers::GetBlockFlag(index);
        }

        void ClearChangesMask()
        {
            std::fill(_updateMask.begin(), _updateMask.end(), 0);
            _changeType = Compat::DynamicFieldChangeType::UNCHANGED;
        }

        std::vector<T> _values;
        std::vector<uint32> _updateMask;
        Compat::DynamicFieldChangeType _changeType{ Compat::DynamicFieldChangeType::UNCHANGED };
    };

    template<typename T, uint32 BlockBit, uint32 Bit>
    class DynamicUpdateField : public DynamicUpdateFieldBase<T>
    {
    };

    template<typename T>
    class OptionalUpdateFieldBase : public IsUpdateFieldHolderTag
    {
        template<typename F, bool PublicSet>
        friend struct MutableFieldReference;

        template<typename F, bool PublicSet>
        friend struct MutableNestedFieldReference;

        template<std::size_t Bits>
        friend class HasChangesMask;

        template<typename F>
        friend struct OptionalUpdateFieldSetter;

    public:
        using value_type = T;
        using IsLarge = std::integral_constant<bool, sizeof(void*) * 3 < sizeof(T)>;
        using StorageType = std::conditional_t<IsLarge::value, std::unique_ptr<T>, Optional<T>>;

        ~OptionalUpdateFieldBase()
        {
            DestroyValue();
        }

        bool has_value() const
        {
            return !!_value;
        }

        operator T const& () const
        {
            return *_value;
        }
        T const* operator->() const
        {
            return &(*_value);
        }
        T const& operator*() const
        {
            return *_value;
        }

    private:
        void ConstructValue()
        {
            ConstructValue(IsLarge{});
        }

        void ConstructValue(std::false_type)
        {
            _value.emplace();
        }

        void ConstructValue(std::true_type)
        {
            _value = std::make_unique<T>();
        }

        void DestroyValue()
        {
            _value.reset();
        }

        StorageType _value = { };
    };

    template<typename T, uint32 BlockBit, uint32 Bit>
    class OptionalUpdateField : public OptionalUpdateFieldBase<T>
    {
    };

    template<typename T>
    struct ViewerDependentValueTag
    {
        using value_type = T;
    };


    namespace Compat
    {
        template<std::size_t Bits>
        class HasDynamicChangesMask : public HasDynamicChangesMaskTag
        {
            template<typename T>
            friend struct UF::DynamicUpdateFieldSetter;

            template<typename T, bool PublicSet>
            friend struct UF::MutableFieldReference;

            template<typename T, uint32 BlockBit, uint32 Bit>
            friend class UF::DynamicUpdateField;

        public:
            using DyBase = HasDynamicChangesMask<Bits>;
            using DyMask = UpdateMask<Bits>;

            template<typename Derived, typename T, uint32 Index, uint32 Offset>
            MutableFieldReference<T, false> ModifyValue(DynamicUpdateField<T, Index, Offset>(Derived::* field))
            {
                MarkChanged(field);
                return { (static_cast<Derived*>(this)->*field)._values };
            }

            template<typename Derived, typename T, uint32 Index, uint32 Offset>
            MutableFieldReference<T, false> ModifyValue(DynamicUpdateField<T, Index, Offset>(Derived::* field), uint32 index)
            {
                DynamicUpdateField<T, Index, Offset>& uf = (static_cast<Derived*>(this)->*field);
                if (index >= uf.size())
                {
                    // fill with zeros until reaching desired slot
                    uf._values.resize(index + 1);
                    uf._updateMask.resize((uf._values.size() + 31) / 32);
                }

                MarkChanged(field);
                (static_cast<Derived*>(this)->*field).MarkChanged(index);
                return { uf._values[index] };
            }


            template<typename Derived, typename T, uint32 Index, uint32 Offset>
            void MarkChanged(Compat::DynamicUpdateField<T, Index, Offset>(Derived::*), uint32)
            {
                static_assert(std::is_base_of<DyBase, Derived>::value, "Given field argument must belong to the same structure as this HasChangesMask");

                _changesDynamicMask.Set(Index);
            }


            DyMask const& GetChangesDynamicMask() const { return _changesDynamicMask; }

        protected:
            DyMask _changesDynamicMask;
        };

        template<typename T, uint32 Index_, uint32 Offset = 0>
        class UpdateField : public UpdateFieldBase<T>
        {
        public:
            static constexpr uint32 Index = Index_;
        };

        template<typename T, std::size_t Size, uint32 Index_, uint32 Offset = 0>
        class UpdateFieldArray : public UpdateFieldArrayBase<T, Size>
        {
        public:
            static constexpr uint32 Index = Index_;
        };

        template<typename T, uint32 Index, uint32 Offset = 0>
        class DynamicUpdateField : public DynamicUpdateFieldBase<T>
        {
        };

        template<typename T, uint32 Index, uint32 Offset = 0>
        class OptionalUpdateField : public OptionalUpdateFieldBase<T>
        {
        };


        // Placeholders - used for fields which may not exist in current client version.
        //TODOFROST - ensure placeholders dont trigger mask changes.
        template<typename T>
        using PlaceholderField = UpdateField<T, 0, 0>;

        template<typename T, std::size_t Size>
        using PlaceholderFieldArray = UpdateFieldArray<T, Size, 0, 0>;

        template<typename T>
        using PlaceholderFieldDynamic = DynamicUpdateField<T, 0, 0>;
    }

}


#endif // UpdateField_h__
