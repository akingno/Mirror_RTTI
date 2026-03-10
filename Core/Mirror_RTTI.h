#ifndef MIRROR_RTTI_H
#define MIRROR_RTTI_H

#include <iostream>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <cstddef>
#include <any>
#include <vector>
#include <functional>


using InstantiateFn = void* (*)();
using DestroyFn = void (*)(void *);
/**
 * 字段描述符
 * 记录单个成员变量的元数据
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


using AnyArgs = const std::vector<std::any> &;
using FunctionWrapper = std::function<std::any(void *, AnyArgs)>;
/**
 * 类描述符
 * 记录类的元数据，并管理其所有字段与生命周期
 * @param name 类名
 * @param type 类的类型信息
 * @param fields_map 通过string查找fieldDesc的哈希表
 * @param functions_map 通过string查function<>的哈希表
 * @param instantor 指向动态创建该类实例的函数指针
 * @param destructor 指向动态销毁该类实例的函数指针
 **/
struct ClassDescriptor {
    std::string name;
    std::type_index type;
    std::unordered_map<std::string, FieldDescriptor> fields_map;
    std::unordered_map<std::string, FunctionWrapper> functions_map;

    InstantiateFn instantiator;
    DestroyFn destructor;

    ClassDescriptor(std::string n, std::type_index t, std::unordered_map<std::string, FieldDescriptor> f,
                    InstantiateFn create_fn, DestroyFn destroy_fn)
        : name(std::move(n)), type(t), fields_map(std::move(f)), instantiator(create_fn), destructor(destroy_fn) {
    }

    void AddField(const std::string &fieldName, std::type_index fieldType, size_t fieldOffset) {
        fields_map.emplace(fieldName, FieldDescriptor{fieldName, fieldType, fieldOffset});
    }

    void AddFunction(const std::string &funcName, FunctionWrapper wrapper) {
        functions_map.emplace(funcName, std::move(wrapper));
    }

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
 **/
class ReflectionRegistry {
public:
    static ReflectionRegistry &Instance() {
        static ReflectionRegistry instance;
        return instance;
    }

    std::unordered_map<std::type_index, ClassDescriptor> classes_map;
    std::unordered_map<std::string, ClassDescriptor *> name_to_class_map;

    ClassDescriptor &RegisterClass(std::type_index type, const std::string &className,
                                   InstantiateFn create_fn = nullptr, DestroyFn destroy_fn = nullptr) {
        auto result = classes_map.emplace(type, ClassDescriptor{className, type, {}, create_fn, destroy_fn});
        ClassDescriptor &desc = result.first->second;
        name_to_class_map[className] = &desc;
        return desc;
    }

    const ClassDescriptor *GetClass(std::type_index type) const {
        auto it = classes_map.find(type);
        return it != classes_map.end() ? &(it->second) : nullptr;
    }

    const ClassDescriptor *GetClassByName(const std::string &className) const {
        auto it = name_to_class_map.find(className);
        return it != name_to_class_map.end() ? it->second : nullptr;
    }

private:
    ReflectionRegistry() = default;
};

/**
 * 设置对象成员变量的值
 * @param instance 对象的首地址
 * @param type 对象类型
 * @param name 要设置的成员变量名
 * @param value 要写入的值
 * @return 是否成功
 */
template<typename T>
bool SetProperty(void *instance, std::type_index type, const std::string &name, const T &value) {
    if (!instance) return false;
    const ClassDescriptor *classDesc = ReflectionRegistry::Instance().GetClass(type);
    if (!classDesc) return false;
    const FieldDescriptor *fieldDesc = classDesc->GetField(name);
    if (!fieldDesc) return false;

    if (fieldDesc->type != typeid(T)) return false;

    char *basePtr = static_cast<char *>(instance);
    T *fieldPtr = reinterpret_cast<T *>(basePtr + fieldDesc->offset);
    *fieldPtr = value;
    return true;
}

/**
 * 获取对象成员变量的值
 * @param instance 对象的首地址
 * @param type 对象的类型
 * @param name 要获取的成员变量名
 * @param outValue 保存结果的引用
 * @return 是否成功
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
 * @param className 要创建对象的类名
 * @return 创建出来的对象首地址，失败返回 nullptr。
 */
inline void *CreateInstanceByName(const std::string &className) {
    const ClassDescriptor *desc = ReflectionRegistry::Instance().GetClassByName(className);
    if (desc && desc->instantiator) {
        return desc->instantiator();
    }
    return nullptr;
}

/**
 * 根据类名字符串，销毁一个通过反射创建的对象
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

/**
 * 动态调用对象的成员函数
 * @param instance 对象的首地址
 * @param type 对象的类型
 * @param funcName 函数名
 * @param args 参数数组，用any包装
 * @return 函数的返回值，用any包装
 */
inline std::any InvokeMethod(void *instance, std::type_index type, const std::string &funcName,
                             const std::vector<std::any> &args = {}) {
    if (!instance) return std::any();
    const ClassDescriptor *desc = ReflectionRegistry::Instance().GetClass(type);
    if (!desc) return std::any();

    auto it = desc->functions_map.find(funcName);
    if (it != desc->functions_map.end()) {
        return it->second(instance, args);
    }

    std::cerr << "[Error] 未找到函数: " << funcName << "\n";
    return std::any();
}

#endif //MIRROR_RTTI_H
