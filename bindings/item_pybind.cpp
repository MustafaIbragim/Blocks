#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <limits>
#include "item.h"
#include "world.h"


namespace py = pybind11;

class PyItem : public Item {
public:
    using Item::Item;

    bool action_0(ItemActionContext& context) const override {
        PYBIND11_OVERRIDE(bool, Item, action_0, context);
    }

    bool action_1(ItemActionContext& context) const override {
        PYBIND11_OVERRIDE(bool, Item, action_1, context);
    }
};


PYBIND11_MODULE(item_module, m) {
    py::module_::import("vec_module");
    py::module_::import("image_module");
    py::module_::import("entity_module");
    py::module_::import("block_module");
    py::module_::import("mesh_module");

    m.doc() = "Items: descriptors, stacks, inventories and item entities";

    py::class_<ItemActionContext>(m, "ItemActionContext")
        .def(py::init<>())
        .def_readwrite("world", &ItemActionContext::world)
        .def_readwrite("stack", &ItemActionContext::stack)
        .def_readwrite("position", &ItemActionContext::position)
        .def_readwrite("block_position", &ItemActionContext::block_position)
        .def_readwrite("block_normal", &ItemActionContext::block_normal)
        .def_readwrite("block_nbt", &ItemActionContext::block_nbt)
        .def_readwrite("has_block", &ItemActionContext::has_block)
        .def("__repr__", [](const ItemActionContext& ctx) {
            return "ItemActionContext(has_block=" +
                   std::string(ctx.has_block ? "true" : "false") + ")";
        });

    py::class_<Item, PyItem, std::shared_ptr<Item>>(m, "Item")
        .def(py::init<>())
        .def(py::init<const std::string&, std::shared_ptr<Image>, uint16_t>(),
             py::arg("name"),
             py::arg("image") = nullptr,
             py::arg("max_stack") = 1)
        .def_readwrite("name", &Item::name)
        .def_readwrite("image", &Item::image)
        .def_readwrite("mesh", &Item::mesh)
        .def_readwrite("block", &Item::block)
        .def_readwrite("stack_limit_code", &Item::stack_limit_code)
        .def_static("create_cube_mesh", &Item::create_cube_mesh,
                    py::arg("size") = 0.8f,
                    "Центрированный куб для 3D-отображения предмета")
        .def_property("max_stack", &Item::max_stack, &Item::set_max_stack)
        .def("set_max_stack", &Item::set_max_stack, py::arg("value"))
        .def("action_0", &Item::action_0, py::arg("context"),
             "Взаимодействие (обычно ПКМ)")
        .def("action_1", &Item::action_1, py::arg("context"),
             "Установка/использование (обычно ЛКМ)")
        .def("__repr__", [](const Item& item) {
            return "Item(name='" + item.name + "', max_stack=" +
                   std::to_string(item.max_stack()) + ")";
        });

    py::class_<ItemStack>(m, "ItemStack")
        .def(py::init<>())
        .def(py::init<std::shared_ptr<Item>, uint16_t>(),
             py::arg("item"), py::arg("count") = 1)
        .def_readwrite("item", &ItemStack::item)
        .def_readwrite("count", &ItemStack::count)
        .def_property_readonly("max_stack", &ItemStack::max_stack)
        .def("empty", &ItemStack::empty)
        .def("same_item", &ItemStack::same_item, py::arg("other"))
        .def("space", &ItemStack::space)
        .def("add", &ItemStack::add, py::arg("amount"))
        .def("remove", &ItemStack::remove, py::arg("amount"))
        .def("clear", &ItemStack::clear)
        .def("__repr__", [](const ItemStack& stack) {
            return "ItemStack(item='" +
                   std::string(stack.item ? stack.item->name : "<empty>") +
                   "', count=" + std::to_string(stack.count) + ")";
        });

    py::class_<Inventory>(m, "Inventory")
        .def(py::init<uint32_t>(), py::arg("slots") = 0)
        .def("size", &Inventory::size)
        .def("resize", &Inventory::resize, py::arg("slots"))
        .def("valid_slot", &Inventory::valid_slot, py::arg("index"))
        .def("get_slot",
             static_cast<ItemStack& (Inventory::*)(uint32_t)>(&Inventory::get_slot),
             py::arg("index"), py::return_value_policy::reference_internal)
        .def("set_slot", &Inventory::set_slot, py::arg("index"), py::arg("stack"))
        .def("add_to_slot",
             [](Inventory& inventory, uint32_t index, ItemStack& stack) {
                 return inventory.add_to_slot(index, stack);
             },
             py::arg("index"), py::arg("stack"),
             "Кладёт стак в конкретную ячейку; возвращает остаток")
        .def("add", &Inventory::add,
             py::arg("stack"),
             py::arg("preferred_slot") = std::numeric_limits<uint32_t>::max())
        .def("take", &Inventory::take,
             py::arg("index"), py::arg("count") = 0)
        .def("can_accept", &Inventory::can_accept, py::arg("stack"))
        .def("count_of", &Inventory::count_of, py::arg("item"))
        .def("empty", &Inventory::empty)
        .def("clear", &Inventory::clear)
        .def("__len__", &Inventory::size)
        .def("__getitem__",
             static_cast<ItemStack& (Inventory::*)(uint32_t)>(&Inventory::get_slot),
             py::arg("index"), py::return_value_policy::reference_internal)
        .def("__repr__", [](const Inventory& inventory) {
            return "Inventory(slots=" + std::to_string(inventory.size()) + ")";
        });

    py::class_<ItemEntity, Entity, std::shared_ptr<ItemEntity>>(m, "ItemEntity")
        .def(py::init<>())
        .def(py::init<const ItemStack&, const Vec3f&, const Vec3f&>(),
             py::arg("stack"),
             py::arg("position") = Vec3f::zero(),
             py::arg("velocity") = Vec3f::zero())
        .def_readwrite("stack", &ItemEntity::stack)
        .def_readwrite("velocity", &ItemEntity::velocity)
        .def_readwrite("pickup_delay", &ItemEntity::pickup_delay)
        .def_readwrite("alive", &ItemEntity::alive)
        .def_readwrite("id", &ItemEntity::id)
        .def_readwrite("sprite_size", &ItemEntity::sprite_size)
        .def_readwrite("render_position", &ItemEntity::render_position)
        .def_property_readonly(
            "collision_body",
            [](ItemEntity& entity) -> FragmentCollision& {
                return entity.collision_body;
            },
            py::return_value_policy::reference_internal)
        .def("can_pickup", &ItemEntity::can_pickup)
        .def("__repr__", [](const ItemEntity& entity) {
            return "ItemEntity(id=" + std::to_string(entity.id) +
                   ", stack=" + std::to_string(entity.stack.count) + ")";
        });
}
