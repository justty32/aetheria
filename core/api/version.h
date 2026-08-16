#pragma once

namespace aetheria {

// core_version 是 core 對外公開的唯讀版本字串。
// 程式映像擁有回傳字元序列，呼叫端只借用指標。
// 指標在程序結束前有效，呼叫端不得修改或釋放。
[[nodiscard]] const char* core_version() noexcept;

}  // namespace aetheria
