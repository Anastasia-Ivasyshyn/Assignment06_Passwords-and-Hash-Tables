#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
using namespace std;

uint64_t demoHash(const string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h = (h ^ c) * 1099511628211ULL;
    }
    return h;
}

struct User {
    string username;
    string salt;
    uint64_t passwordHash;
};

// The four methods below all use the same simple key hash.
size_t indexFor(const string& key, size_t capacity) {
    return demoHash(key) % capacity;
}

class HashTable {
    struct Node {
        User user;
        Node* next;
        Node(User u, Node* n) : user(u), next(n) {}
    };
    vector<Node*> buckets;
    size_t count = 0;

    void rehash() {
        size_t newCapacity = buckets.size() * 2 + 1;
        vector<Node*> newBuckets(newCapacity, nullptr);
        for (Node* head : buckets) {
            while (head != nullptr) {
                Node* next = head->next;
                size_t index = indexFor(head->user.username, newCapacity);
                head->next = newBuckets[index];
                newBuckets[index] = head;
                head = next;
            }
        }
        buckets.swap(newBuckets);
        cout << "Rehash: table grew to " << buckets.size() << " buckets\n";
    }
public:
    HashTable() : buckets(3, nullptr) {}
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;
    ~HashTable() {
        for (Node* head : buckets) {
            while (head != nullptr) {
                Node* next = head->next;
                delete head;
                head = next;
            }
        }
    }
    double loadFactor() const { return double(count) / buckets.size(); }
    size_t size() const { return count; }
    size_t capacity() const { return buckets.size(); }

    User* find(const string& username) {
        size_t index = indexFor(username, buckets.size());
        for (Node* p = buckets[index]; p != nullptr; p = p->next)
            if (p->user.username == username) return &p->user;
        return nullptr;
    }
    bool insert(const User& user) {
        if (find(user.username) != nullptr) return false;
        if (double(count + 1) / buckets.size() > 0.75) rehash();
        size_t index = indexFor(user.username, buckets.size());
        if (buckets[index] != nullptr)
            cout << "Collision in bucket " << index << " for " << user.username << "\n";
        buckets[index] = new Node(user, buckets[index]);
        ++count;
        return true;
    }
    bool remove(const string& username) {
        size_t index = indexFor(username, buckets.size());
        Node* current = buckets[index];
        Node* previous = nullptr;
        while (current != nullptr) {
            if (current->user.username == username) {
                if (previous == nullptr) buckets[index] = current->next;
                else previous->next = current->next;
                delete current;
                --count;
                return true;
            }
            previous = current;
            current = current->next;
        }
        return false;
    }
};

// Open addressing comparison. 0 = linear, 1 = quadratic, 2 = double hashing.
class ProbingTable {
    struct Slot {
        string key;
        bool used = false;
        bool deleted = false;
    };
    vector<Slot> slots;
    int method;
    size_t count = 0;
    size_t collisions = 0;
    size_t resizeCount = 0;

    size_t position(const string& key, size_t attempt) const {
        size_t n = slots.size();
        size_t first = indexFor(key, n);
        if (method == 0) return (first + attempt) % n;
        if (method == 1) return (first + attempt * attempt) % n;
        size_t step = 1 + demoHash("step:" + key) % (n - 1);
        return (first + attempt * step) % n;
    }
    // A prime capacity makes double hashing's step visit all buckets.
    // Quadratic probing is kept below 50% load so it can find a free slot.
    void rehash() {
        vector<Slot> old = slots;
        slots = vector<Slot>(slots.size() * 2 + 1);
        // The sizes used here (11, 23, 47, ...) are prime.
        count = 0;
        ++resizeCount;
        for (const Slot& s : old) if (s.used) insert(s.key);
    }
public:
    ProbingTable(int m) : slots(11), method(m) {}
    double loadFactor() const { return double(count) / slots.size(); }
    size_t collisionCount() const { return collisions; }
    size_t rehashCount() const { return resizeCount; }
    size_t capacity() const { return slots.size(); }
    bool find(const string& key) const {
        for (size_t i = 0; i < slots.size(); ++i) {
            const Slot& s = slots[position(key, i)];
            if (!s.used && !s.deleted) return false;
            if (s.used && s.key == key) return true;
        }
        return false;
    }
    bool insert(const string& key) {
        if (find(key)) return false;
        if (double(count + 1) / slots.size() > 0.45) rehash();
        for (size_t i = 0; i < slots.size(); ++i) {
            Slot& s = slots[position(key, i)];
            if (s.used) ++collisions;
            if (!s.used) {
                s.key = key;
                s.used = true;
                s.deleted = false;
                ++count;
                return true;
            }
        }
        return false;
    }
    bool remove(const string& key) {
        for (size_t i = 0; i < slots.size(); ++i) {
            Slot& s = slots[position(key, i)];
            if (!s.used && !s.deleted) return false;
            if (s.used && s.key == key) {
                s.used = false;
                s.deleted = true;
                --count;
                return true;
            }
        }
        return false;
    }
};

const string PEPPER = "class-demo-pepper";

User makeUser(const string& name, const string& password, const string& salt) {
    return {name, salt, demoHash(salt + password + PEPPER)};
}

void login(HashTable& table, const string& name, const string& password) {
    User* user = table.find(name);
    if (user == nullptr) {
        cout << "Login " << name << ": unknown user\n";
    } else if (demoHash(user->salt + password + PEPPER) == user->passwordHash) {
        cout << "Login " << name << ": success\n";
    } else {
        cout << "Login " << name << ": wrong password\n";
    }
}

int main() {
    cout << "COLLISION STRATEGIES\n";
    string names[] = {"alice", "bob", "charlie", "diana", "emma", "frank"};
    string methods[] = {"Linear probing", "Quadratic probing", "Double hashing"};
    // Find three different usernames that hash to the same starting slot.
    // This guarantees that each probing method has to resolve a collision.
    vector<string> collidingNames;
    size_t targetBucket = indexFor("test0", 11);
    for (int i = 0; collidingNames.size() < 3; ++i) {
        string candidate = "test" + to_string(i);
        if (indexFor(candidate, 11) == targetBucket)
            collidingNames.push_back(candidate);
    }
    cout << "Collision test: " << collidingNames[0] << ", "
         << collidingNames[1] << ", " << collidingNames[2]
         << " all start in bucket " << targetBucket << "\n";

    for (int m = 0; m < 3; ++m) {
        ProbingTable table(m);
        for (const string& name : collidingNames) table.insert(name);
        cout << methods[m] << ": collisions=" << table.collisionCount()
             << ", all found=" << (table.find(collidingNames[0]) &&
                                  table.find(collidingNames[1]) &&
                                  table.find(collidingNames[2])) << "\n";
        cout << "  Remove middle=" << table.remove(collidingNames[1])
             << ", other keys still found="
             << (table.find(collidingNames[0]) && table.find(collidingNames[2]))
             << ", removed key missing=" << !table.find(collidingNames[1]) << "\n";
        for (const string& name : names) table.insert(name);
        cout << "  After more inserts: capacity=" << table.capacity()
             << ", rehashes=" << table.rehashCount()
             << ", load=" << table.loadFactor() << "\n";
    }

    cout << "\nAUTHENTICATION WITH SEPARATE CHAINING\n";
    HashTable users;
    users.insert(makeUser("alice", "apple123", "salt-A"));
    users.insert(makeUser("bob", "banana456", "salt-B"));
    users.insert(makeUser("charlie", "cherry789", "salt-C"));
    users.insert(makeUser("diana", "dragon111", "salt-D"));
    users.insert(makeUser("emma", "earth222", "salt-E"));
    users.insert(makeUser("frank", "forest333", "salt-F"));
    cout << "Users=" << users.size() << ", buckets=" << users.capacity()
         << ", load factor=" << users.loadFactor() << "\n";
    login(users, "alice", "apple123");
    login(users, "alice", "wrong123");
    login(users, "nobody", "anything");
    cout << "Remove bob: " << users.remove("bob") << "\n";
    login(users, "bob", "banana456");
    cout << "Users after removal=" << users.size() << "\n";
}
