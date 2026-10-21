package com.vdcs.mobile.data.protocol

class ByteReader(private val bytes: ByteArray) {
    fun u8(offset: Int): Int = bytes[offset].toInt() and 0xFF

    fun u16(offset: Int): Int = u8(offset) or (u8(offset + 1) shl 8)

    fun i16(offset: Int): Int = u16(offset).toShort().toInt()

    fun u32(offset: Int): Long =
        (u8(offset).toLong()) or
            (u8(offset + 1).toLong() shl 8) or
            (u8(offset + 2).toLong() shl 16) or
            (u8(offset + 3).toLong() shl 24)

    fun u64(offset: Int): Long = u32(offset) or (u32(offset + 4) shl 32)
}

class ByteWriter(size: Int) {
    private val bytes = ByteArray(size)

    fun u8(offset: Int, value: Int) = apply { bytes[offset] = value.toByte() }

    fun u16(offset: Int, value: Int) = apply {
        u8(offset, value and 0xFF)
        u8(offset + 1, (value ushr 8) and 0xFF)
    }

    fun i16(offset: Int, value: Int) = u16(offset, value and 0xFFFF)

    fun u32(offset: Int, value: Long) = apply {
        for (i in 0 until 4) u8(offset + i, ((value ushr (8 * i)) and 0xFF).toInt())
    }

    fun u64(offset: Int, value: Long) = apply {
        for (i in 0 until 8) u8(offset + i, ((value ushr (8 * i)) and 0xFF).toInt())
    }

    fun bytes(offset: Int, src: ByteArray) = apply { src.copyInto(bytes, offset) }

    fun toByteArray(): ByteArray = bytes
}
