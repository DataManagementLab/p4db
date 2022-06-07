#include "mbufrawpacket.hpp"

#include "device.hpp"


DPDKPacket* DPDKPacket::wrap(struct rte_mbuf* mbuf) {
    return static_cast<DPDKPacket*>(mbuf);
}


struct rte_mbuf* DPDKPacket::raw() {
    return static_cast<struct rte_mbuf*>(this);
}


uint8_t* DPDKPacket::reserve(uint16_t len) {
    if (unlikely(len > RTE_MBUF_DEFAULT_DATAROOM)) { // 2048
        throw std::invalid_argument("len > RTE_MBUF_DEFAULT_DATAROOM, currently only one segment mbufs supported");
    }

    uint16_t current_len = this->size();

    uint8_t* data;
    if (current_len < len) {
        data = (uint8_t*)rte_pktmbuf_append(this, len - current_len);
    } else if (current_len > len) {
        if (unlikely(rte_pktmbuf_trim(this, current_len - len) != 0)) {
            throw std::bad_alloc();
        }
        data = rte_pktmbuf_mtod(this, uint8_t*);
    } else {
        data = rte_pktmbuf_mtod(this, uint8_t*);
    }

    if (unlikely(!data)) {
        throw std::bad_alloc();
    }

    return data;
}

// void DPDKPacket::trim(uint16_t len) {
// 	struct rte_mbuf *m_last;

// 	if (unlikely(len > this->data_len)) {
// 		throw std::invalid_argument("pkt trim failed because requested size is larger");
// 	}

// 	this->data_len = (uint16_t)(this->data_len - len);
// 	this->pkt_len  = (this->pkt_len - len);
// }

uint8_t* DPDKPacket::data() {
    return rte_pktmbuf_mtod(this, uint8_t*);
}

uint16_t DPDKPacket::size() {
    return rte_pktmbuf_pkt_len(this);
}

void DPDKPacket::free() {
    rte_pktmbuf_free(this);
}
