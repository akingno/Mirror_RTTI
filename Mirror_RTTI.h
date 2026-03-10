//
// Created by jacob on 26-3-10.
//

#ifndef MIRROR_RTTI_H
#define MIRROR_RTTI_H

#include <string>
#include <unordered_map>
#include <typeindex>
#include <cstddef>

#define BEGIN_CLASS(CLASS_NAME) \
    namespace { \
        struct AutoReg_##CLASS_NAME { \
            static void* Instantlize() { return new CLASS_NAME(); } \
            static void Destroy(void* ptr) { delete static_cast<CLASS_NAME*>(ptr); } \
            AutoReg_##CLASS_NAME() { \
                using CurrentClass = CLASS_NAME; \
                auto& desc = ReflectionRegistry::Instance().RegisterClass( \
                    typeid(CLASS_NAME), #CLASS_NAME, \
                    &AutoReg_##CLASS_NAME::Instantlize, \
                    &AutoReg_##CLASS_NAME::Destroy);

#define REGISTER_FIELD(FIELD_NAME) \
    desc.AddField(#FIELD_NAME, typeid(decltype(((CurrentClass*)nullptr)->FIELD_NAME)), offsetof(CurrentClass, FIELD_NAME));

#define END_CLASS(CLASS_NAME) \
            } \
        }; \
        static AutoReg_##CLASS_NAME global_AutoReg_Instance_##CLASS_NAME; \
    }

/**
 * 字段描述符
 * @param name 成员变量名
 * @param type 类型
 * @param offset 内存偏移量
 **/
struct FieldDescriptor {
    std::string name;
    std::type_index type;
    size_t offset;

    FieldDescriptor(std::string n, std::type_index t, size_t o)
        : name(std::move(n)), type(t), offset(o) {
    }
};


using InstantFn = void* (*)();
using DestroyFn = void (*)(void *);
/**
 * 类描述符
 * @param name 类名
 * @param type 类的类型信息
 * @param fields_map 通过string找到field
 *
 **/
struct ClassDescriptor {
    std::string name;
    std::type_index type;
    std::unordered_map<std::string, FieldDescriptor> fields_map;
    InstantFn instantor;
    DestroyFn destructor;

    ClassDescriptor(std::string n, std::type_index t, std::unordered_map<std::string, FieldDescriptor> f, InstantFn fn,
                    DestroyFn destroy_fn)
        : name(std::move(n)), type(t), fields_map(std::move(f)), instantor(fn), destructor(destroy_fn) {
    }


    // 添加字段
    void AddField(const std::string &fieldName, std::type_index fieldType, size_t fieldOffset) {
        fields_map.emplace(fieldName, FieldDescriptor{fieldName, fieldType, fieldOffset});
    }

    // 获取字段
    const FieldDescriptor *GetField(const std::string &fieldName) const {
        auto it = fields_map.find(fieldName);
        if (it != fields_map.end()) {
            return &(it->second);
        }
        return nullptr;
    }
};

/**
 * 全局注册表：管理系统中所有的反射信息
 * 单例模式
 * @param classes_map typeid -> 类描述符
 *
 **/
class ReflectionRegistry {
public:
    static ReflectionRegistry &Instance() {
        static ReflectionRegistry instance;
        return instance;
    }

    // 类型 -> 类描述
    std::unordered_map<std::type_index, ClassDescriptor> classes_map;

    // String -> 类描述ptr
    std::unordered_map<std::string, ClassDescriptor *> name_to_class_map;

    // 注册一个新类
    ClassDescriptor &RegisterClass(std::type_index type, const std::string &className, InstantFn fn, DestroyFn d_fn) {
        auto result = classes_map.emplace(type, ClassDescriptor{className, type, {}, fn, d_fn});
        ClassDescriptor &desc = result.first->second;
        name_to_class_map[className] = &desc;
        return desc;
    }

    // 查找已注册的类
    const ClassDescriptor *GetClass(std::type_index type) const {
        auto it = classes_map.find(type);
        if (it != classes_map.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    const ClassDescriptor *GetClassByName(const std::string &className) const {
        auto it = name_to_class_map.find(className);
        if (it != name_to_class_map.end()) {
            return it->second;
        }
        return nullptr;
    }

private:
    ReflectionRegistry() = default;
};

/**
 * 动态设置对象成员变量的值
 * @param instance 对象的首地址
 * @param type 对象的类型信息
 * @param name 要设置的成员变量名
 * @param value 要写入的值
 * @return 设置是否成功
 */
template<typename T>
bool SetProperty(void *instance, std::type_index type, const std::string &name, const T &value) {
    if (!instance) return false;

    // 获取类描述符
    const ClassDescriptor *classDesc = ReflectionRegistry::Instance().GetClass(type);
    if (!classDesc) return false;

    // 获取字段描述符
    const FieldDescriptor *fieldDesc = classDesc->GetField(name);
    if (!fieldDesc) return false;

    // check 传入的类型是否和注册的类型一致
    if (fieldDesc->type != typeid(T)) {
        std::cerr << "[Error] 类型不匹配，字段 '" << name << "' ，需要的类型： "
                << fieldDesc->type.name() << "，但传入了 " << typeid(T).name() << "\n";
        return false;
    }

    // 将void*转换为char*以便按字节进行偏移
    char *basePtr = static_cast<char *>(instance);

    // 找到目标内存地址
    char *targetAddr = basePtr + fieldDesc->offset;

    // 强转该地址为目标类型的指针
    T *fieldPtr = reinterpret_cast<T *>(targetAddr);

    *fieldPtr = value;

    return true;
}

/**
 * 动态获取对象成员变量的值
 * @param instance 对象的首地址
 * @param type  对象的类型信息
 * @param name  要获取的成员变量名
 * @param outValue 用于接收结果的变量引用
 * @return         获取是否成功
 */
template<typename T>
bool GetProperty(void *instance, std::type_index type, const std::string &name, T &outValue) {
    if (!instance) return false;

    const ClassDescriptor *classDesc = ReflectionRegistry::Instance().GetClass(type);
    if (!classDesc) return false;

    const FieldDescriptor *fieldDesc = classDesc->GetField(name);
    if (!fieldDesc) return false;

    if (fieldDesc->type != typeid(T)) return false;

    char *basePtr = static_cast<char *>(instance);
    T *fieldPtr = reinterpret_cast<T *>(basePtr + fieldDesc->offset);

    outValue = *fieldPtr;
    return true;
}

/**
 * 根据类名字符串，创建一个对象
 * @param className 类名字符串
 * @return 创建的对象首地址。失败返回 nullptr。
 */
inline void *CreateInstanceByName(const std::string &className) {
    const ClassDescriptor *desc = ReflectionRegistry::Instance().GetClassByName(className);
    if (desc && desc->instantor) {
        return desc->instantor();
    }
    return nullptr;
}

/**
 * 根据类名字符串，安全销毁一个通过反射创建的对象
 * @param instance 要销毁的对象首地址
 * @param className 类名字符串
 */
inline void DestroyInstanceByName(void *instance, const std::string &className) {
    if (!instance) return;

    const ClassDescriptor *desc = ReflectionRegistry::Instance().GetClassByName(className);
    if (desc && desc->destructor) {
        desc->destructor(instance);
    }
}

#endif //MIRROR_RTTI_H
