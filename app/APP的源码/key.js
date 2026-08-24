/**
 * OneNET Token 生成 —— 纯 JS 实现 (适配 uni-app，不依赖 Node.js crypto/Buffer)
 */

/* ========== Base64 编解码 ========== */
var BASE64_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=';

function base64ToBytes(b64) {
	b64 = b64.replace(/[^A-Za-z0-9\+\/\=]/g, '');
	var bytes = [];
	var i = 0;
	while (i < b64.length) {
		var a = BASE64_CHARS.indexOf(b64.charAt(i++));
		var b = BASE64_CHARS.indexOf(b64.charAt(i++));
		var c = BASE64_CHARS.indexOf(b64.charAt(i++));
		var d = BASE64_CHARS.indexOf(b64.charAt(i++));
		bytes.push((a << 2) | (b >> 4));
		if (c !== 64) bytes.push(((b & 15) << 4) | (c >> 2));
		if (d !== 64) bytes.push(((c & 3) << 6) | d);
	}
	return bytes;
}

function bytesToBase64(bytes) {
	var b64 = '';
	for (var i = 0; i < bytes.length; i += 3) {
		var a = bytes[i];
		var b = i + 1 < bytes.length ? bytes[i + 1] : NaN;
		var c = i + 2 < bytes.length ? bytes[i + 2] : NaN;
		b64 += BASE64_CHARS.charAt(a >> 2);
		b64 += BASE64_CHARS.charAt(((a & 3) << 4) | (isNaN(b) ? 0 : (b >> 4)));
		b64 += isNaN(b) ? '=' : BASE64_CHARS.charAt(((b & 15) << 2) | (isNaN(c) ? 0 : (c >> 6)));
		b64 += isNaN(c) ? '=' : BASE64_CHARS.charAt(c & 63);
	}
	return b64;
}

/* ========== UTF-8 编码 (字符串 → 字节数组) ========== */
function stringToUtf8Bytes(str) {
	var bytes = [];
	for (var i = 0; i < str.length; i++) {
		var c = str.charCodeAt(i);
		if (c < 0x80) {
			bytes.push(c);
		} else if (c < 0x800) {
			bytes.push(0xc0 | (c >> 6), 0x80 | (c & 0x3f));
		} else if (c < 0xd800 || c >= 0xe000) {
			bytes.push(0xe0 | (c >> 12), 0x80 | ((c >> 6) & 0x3f), 0x80 | (c & 0x3f));
		} else {
			// surrogate pair
			i++;
			var c2 = str.charCodeAt(i);
			var cp = 0x10000 + ((c & 0x3ff) << 10) + (c2 & 0x3ff);
			bytes.push(
				0xf0 | (cp >> 18),
				0x80 | ((cp >> 12) & 0x3f),
				0x80 | ((cp >> 6) & 0x3f),
				0x80 | (cp & 0x3f)
			);
		}
	}
	return bytes;
}

/* ========== SHA1 ========== */
function sha1(bytes) {
	function rol(n, s) { return ((n << s) | (n >>> (32 - s))) >>> 0; }

	var ml = bytes.length * 8;
	var pad = [0x80];
	while ((bytes.length + pad.length) * 8 % 512 !== 448) pad.push(0);
	// 8-byte big-endian length
	pad.push(0, 0, 0, 0, (ml >>> 24) & 0xff, (ml >>> 16) & 0xff, (ml >>> 8) & 0xff, ml & 0xff);
	bytes = bytes.concat(pad);

	var h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;

	for (var block = 0; block < bytes.length; block += 64) {
		var w = [];
		for (var i = 0; i < 16; i++) {
			w[i] = (bytes[block + i * 4] << 24) | (bytes[block + i * 4 + 1] << 16) |
			       (bytes[block + i * 4 + 2] << 8) | bytes[block + i * 4 + 3];
		}
		for (var i = 16; i < 80; i++) {
			w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
		}

		var a = h0, b = h1, c = h2, d = h3, e = h4;

		for (var i = 0; i < 80; i++) {
			var f, k;
			if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999; }
			else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1; }
			else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
			else             { f = b ^ c ^ d;                    k = 0xCA62C1D6; }

			var temp = (rol(a, 5) + f + e + k + w[i]) >>> 0;
			e = d; d = c; c = rol(b, 30); b = a; a = temp;
		}

		h0 = (h0 + a) >>> 0; h1 = (h1 + b) >>> 0;
		h2 = (h2 + c) >>> 0; h3 = (h3 + d) >>> 0; h4 = (h4 + e) >>> 0;
	}

	// 输出为字节数组 (big-endian)
	var out = [];
	[h0, h1, h2, h3, h4].forEach(function(v) {
		out.push((v >>> 24) & 0xff, (v >>> 16) & 0xff, (v >>> 8) & 0xff, v & 0xff);
	});
	return out;
}

/* ========== HMAC-SHA1 ========== */
function hmacSha1(keyBytes, msg) {
	var blockSize = 64;
	// 如果 key 长于 blockSize，先 hash 一次
	if (keyBytes.length > blockSize) {
		keyBytes = sha1(keyBytes);
	}
	// 补零到 blockSize
	while (keyBytes.length < blockSize) keyBytes.push(0);

	var oKeyPad = [], iKeyPad = [];
	for (var i = 0; i < blockSize; i++) {
		oKeyPad.push(keyBytes[i] ^ 0x5c);
		iKeyPad.push(keyBytes[i] ^ 0x36);
	}

	var msgBytes = stringToUtf8Bytes(msg);
	var inner = sha1(iKeyPad.concat(msgBytes));
	return sha1(oKeyPad.concat(inner));
}

/* ========== 主函数: 生成 OneNET Token ========== */
function createCommonToken(params) {
	// 1. Base64 解码 access_key → 字节数组
	var accessKeyBytes = base64ToBytes(params.access_key);

	// 2. 构建要签名的 key
	var res = 'products/' + params.productid;
	var et = Math.ceil((Date.now() + 365 * 24 * 3600 * 1000) / 1000);
	var method = 'sha1';
	var keyStr = et + '\n' + method + '\n' + res + '\n' + params.version;

	// 3. HMAC-SHA1 签名，输出 Base64
	var signBytes = hmacSha1(accessKeyBytes, keyStr);
	var sign = bytesToBase64(signBytes);

	// 4. URL 编码并拼接 token
	res = encodeURIComponent(res);
	sign = encodeURIComponent(sign);
	var token = 'version=' + params.version + '&res=' + res + '&et=' + et + '&method=' + method + '&sign=' + sign;

	return token;
}

module.exports = {
	createCommonToken: createCommonToken
};
