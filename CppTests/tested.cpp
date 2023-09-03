#include <iostream>
#include <vector>
#include <iostream>
#include <concepts>
#include <utility>
#include <type_traits>
#include <iterator>
#include <optional>

class mystr {
public:
	mystr(const char* c_string) {
		while (*c_string != '\0') {
			m_chars.push_back(*c_string);
			++c_string;
		}
		m_chars.push_back('\0');
	}

	auto begin() {
		return m_chars.begin();
	}

	auto end() {
		return begin() + (m_chars.size() - 1);
	}

	auto rbegin() {
		return m_chars.rbegin() + 1;
	}

	auto rend() {
		return rbegin() + (m_chars.size() - 1);
	}

	auto cbegin() const {
		return m_chars.cbegin();
	}

	auto cend() const {
		return cbegin() + (m_chars.size() - 1);
	}

	auto crbegin() const {
		return m_chars.crbegin() + 1;
	}

	auto crend() const {
		return crbegin() + (m_chars.size() - 1);
	}

	auto data() {
		return m_chars.data();
	}

	auto c_str() {
		return m_chars.data();
	}

	operator const char* () {
		return c_str();
	}

	auto& operator+=(const mystr& other) {
		m_chars.pop_back();
		m_chars.insert(m_chars.end(), other.m_chars.begin(), other.m_chars.end());

		return *this;
	}

	auto operator+(const mystr& other) {
		mystr mycopy = *this;

		mycopy += other;

		return mycopy;
	}

	auto& operator[](size_t index) {
		return m_chars[index];
	}
private:
	std::vector<char> m_chars;
};

template<typename T>
concept hashable = std::default_initializable<T> && requires(T t) {
	{ std::hash<T>()(t) } -> std::same_as<std::size_t>;
};

template<typename T>
concept is_value = std::default_initializable<T>;

template<hashable TKey, std::default_initializable TValue>
struct HTableBucket {
	TKey key;
	TValue value;
	bool used;
	bool tombstone;
};

template<hashable TKey, is_value TValue>
class HTable {
	using Bucket = HTableBucket<TKey, TValue>;
	using KeyParam = const TKey;
	using ValParam = const TValue;
public:
	struct Iterator {
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = std::pair<const TKey&, TValue&>;
		using pointer = value_type*;
		using reference = value_type&;

		Iterator(Bucket* bucket, Bucket* end_bucket) :
			m_bucket(bucket), m_end_bucket(end_bucket) {
			while (m_bucket != m_end_bucket && !m_bucket->used) {
				++m_bucket;
			}
			if (m_bucket != m_end_bucket) {
				m_cur_pair.emplace(_bucket_to_pair(bucket));
			}
		}

		reference operator*() {
			return m_cur_pair.value();
		}

		pointer operator->() {
			return &m_cur_pair.value();
		}

		Iterator& operator++() {
			++m_bucket;
			while (m_bucket != m_end_bucket && !m_bucket->used) {
				++m_bucket;
			}
			m_cur_pair.emplace(_bucket_to_pair(m_bucket));
			return *this;
		}

		Iterator& operator++(int) {
			Iterator tmp = *this;
			++tmp;
			return tmp;
		}

		bool operator==(const Iterator& other) const {
			return m_bucket == other.m_bucket;
		}

		bool operator!=(const Iterator& other) const {
			return m_bucket != other.m_bucket;
		}
	private:
		value_type _bucket_to_pair(Bucket* bucket) {
			return std::pair<const TKey&, TValue&>(bucket->key, bucket->value);
		}

		std::optional<value_type> m_cur_pair;
		Bucket* m_bucket;
		Bucket* m_end_bucket;
	};

	bool insert(KeyParam key, ValParam value) {
		Bucket& bucket = _find_bucket(key);

		if (bucket.used) {
			bucket.value = value;

			return false;
		}

		if (_incr_count_check_ld_factor()) {
			Bucket& new_bucket = _find_bucket(key);

			new_bucket.used = true;
			new_bucket.key = key;
			new_bucket.value = value;
		}
		else {
			bucket.used = true;
			bucket.key = key;
			bucket.value = value;
		}

		return true;
	}

	bool contains(KeyParam key) {
		Bucket& bucket = _find_bucket(key);

		return bucket.used;
	}

	bool erase(KeyParam key) {
		Bucket& bucket = _find_bucket(key);

		if (bucket.used) {
			bucket.used = false;
			bucket.tombstone = true;
			bucket.value = TValue();

			return true;
		}

		return false;
	}

	TValue& operator[](KeyParam key) {
		Bucket& bucket = _find_bucket(key);

		if (!bucket.used) {
			bucket.used = true;
			bucket.tombstone = false;
			if (_incr_count_check_ld_factor()) {
				bucket = _find_bucket(key);
			}
		}

		bucket.key = key;
		return bucket.value;
	}

	Iterator begin() {
		return Iterator(m_buckets.data(), m_buckets.data() + m_buckets.size());
	}

	Iterator end() {
		return Iterator(m_buckets.data() + m_buckets.size(), m_buckets.data() + m_buckets.size());
	}

private:
	int m_val_count;
	std::vector<Bucket> m_buckets;

	Bucket& _find_bucket(KeyParam key) {
		if (m_buckets.empty()) {
			m_buckets.resize(1);
		}

		size_t bucket_count = m_buckets.size();
		size_t hash_val = std::hash<TKey>()(key) % bucket_count;

		Bucket* b = &m_buckets[hash_val];
		while ((key != b->key) && (b->used || b->tombstone)) {
			if (++hash_val >= bucket_count) {
				hash_val = 0;
			}

			b = &m_buckets[hash_val];
		}

		return *b;
	}

	bool _incr_count_check_ld_factor() {
		++m_val_count;
		if (m_val_count / static_cast<double>(m_buckets.size()) > 0.7) {
			std::vector<Bucket> new_buckets(m_buckets.size() * 2 + 1);
			std::swap(m_buckets, new_buckets);

			for (Bucket& bucket : new_buckets) {
				if (bucket.used) {
					insert(bucket.key, bucket.value);
				}
			}

			return true;
		}

		return false;
	}
};

template<hashable TKey>
class HTableSet {
public:
	bool insert(TKey key) {
		return m_htable.insert(key, 0);
	}

	bool contains(TKey key) {
		return m_htable.contains(key);
	}

	bool erase(TKey key) {
		return m_htable.erase(key);
	}
private:
	HTable<TKey, char> m_htable;
};
