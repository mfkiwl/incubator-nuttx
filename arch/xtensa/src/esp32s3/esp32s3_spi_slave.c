/****************************************************************************
 * arch/xtensa/src/esp32s3/esp32s3_spi_slave.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_ESP32S3_SPI) && defined(CONFIG_SPI_SLAVE)

#include <assert.h>
#include <debug.h>
#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/semaphore.h>
#include <nuttx/spinlock.h>
#include <nuttx/spi/spi.h>
#include <nuttx/spi/slave.h>

#include <arch/board/board.h>

#include "esp32s3_spi.h"
#include "esp32s3_irq.h"
#include "esp32s3_gpio.h"

#ifdef CONFIG_ESP32S3_SPI_DMA
#include "esp32s3_dma.h"
#endif

#include "xtensa.h"
#include "hardware/esp32s3_gpio_sigmap.h"
#include "hardware/esp32s3_pinmap.h"
#include "hardware/esp32s3_spi.h"
#include "hardware/esp32s3_soc.h"
#include "hardware/esp32s3_system.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

#define SPI_SLAVE_BUFSIZE (CONFIG_ESP32S3_SPI_SLAVE_BUFSIZE)

#ifdef CONFIG_ESP32S3_SPI_DMA
/* SPI DMA RX/TX number of descriptors */

#  if (SPI_SLAVE_BUFSIZE % ESP32S3_DMA_BUFLEN_MAX) > 0
#    define SPI_DMA_DESC_NUM (SPI_SLAVE_BUFSIZE / ESP32S3_DMA_BUFLEN_MAX + 1)
#  else
#    define SPI_DMA_DESC_NUM (SPI_SLAVE_BUFSIZE / ESP32S3_DMA_BUFLEN_MAX)
#  endif

#  define SPI_SLV_INT_EN      (SPI_SLV_WR_DMA_DONE_INT_ENA_M | SPI_SLV_RD_DMA_DONE_INT_ENA_M)
#  define SPI_SLV_INT_RX      SPI_SLV_WR_DMA_DONE_INT_ST_M
#  define SPI_SLV_INT_CLR_RX  SPI_SLV_WR_DMA_DONE_INT_CLR_M
#  define SPI_SLV_INT_TX      SPI_SLV_RD_DMA_DONE_INT_ST_M
#  define SPI_SLV_INT_CLR_TX  SPI_SLV_RD_DMA_DONE_INT_CLR_M
#else
#  define SPI_SLV_INT_EN      (SPI_SLV_WR_BUF_DONE_INT_ENA_M | SPI_SLV_RD_BUF_DONE_INT_ENA_M)
#  define SPI_SLV_INT_RX      SPI_SLV_WR_BUF_DONE_INT_ST_M
#  define SPI_SLV_INT_CLR_RX  SPI_SLV_WR_BUF_DONE_INT_CLR_M
#  define SPI_SLV_INT_TX      SPI_SLV_RD_BUF_DONE_INT_ST_M
#  define SPI_SLV_INT_CLR_TX  SPI_SLV_RD_BUF_DONE_INT_CLR_M
#endif /* CONFIG_ESP32S3_SPI_DMA */

/* Verify whether SPI has been assigned IOMUX pins.
 * Otherwise, SPI signals will be routed via GPIO Matrix.
 */

#ifdef CONFIG_ESP32S3_SPI2
#  define SPI_IS_CS_IOMUX   (CONFIG_ESP32S3_SPI2_CSPIN == SPI2_IOMUX_CSPIN)
#  define SPI_IS_CLK_IOMUX  (CONFIG_ESP32S3_SPI2_CLKPIN == SPI2_IOMUX_CLKPIN)
#  define SPI_IS_MOSI_IOMUX (CONFIG_ESP32S3_SPI2_MOSIPIN == SPI2_IOMUX_MOSIPIN)
#  define SPI_IS_MISO_IOMUX (CONFIG_ESP32S3_SPI2_MISOPIN == SPI2_IOMUX_MISOPIN)

/* In quad SPI mode, flash's IO map is:
 *    MOSI -> IO0
 *    MISO -> IO1
 *    WP   -> IO2
 *    Hold -> IO3
 */

#  ifdef CONFIG_ESP32S3_SPI_IO_QIO
#    define SPI_IS_IO2_IOMUX  (CONFIG_ESP32S3_SPI2_IO2PIN == SPI2_IOMUX_WPPIN)
#    define SPI_IS_IO3_IOMUX  (CONFIG_ESP32S3_SPI2_IO3PIN == SPI2_IOMUX_HDPIN)

#    define SPI_VIA_IOMUX     ((SPI_IS_CS_IOMUX) && \
                               (SPI_IS_CLK_IOMUX) && \
                               (SPI_IS_MOSI_IOMUX) && \
                               (SPI_IS_MISO_IOMUX) && \
                               (SPI_IS_IO2_IOMUX) && \
                               (SPI_IS_IO3_IOMUX))
#  else
#    define SPI_VIA_IOMUX     ((SPI_IS_CS_IOMUX) && \
                               (SPI_IS_CLK_IOMUX) && \
                               (SPI_IS_MOSI_IOMUX) && \
                               (SPI_IS_MISO_IOMUX))
#  endif
#else
#  define SPI_VIA_IOMUX     0
#endif

/* SPI Slave interrupt mask */

#define SPI_INT_MASK      (SPI_TRANS_DONE_INT_ENA_M |      \
                           SPI_SLV_WR_DMA_DONE_INT_ENA_M | \
                           SPI_SLV_RD_DMA_DONE_INT_ENA_M | \
                           SPI_SLV_WR_BUF_DONE_INT_ENA_M | \
                           SPI_SLV_RD_BUF_DONE_INT_ENA_M)

/* SPI Slave default width */

#define SPI_SLAVE_DEFAULT_WIDTH (8)

/* SPI Slave default mode */

#define SPI_SLAVE_DEFAULT_MODE  (SPISLAVE_MODE0)

/* SPI Slave maximum buffer size in bytes */

#define SPI_SLAVE_HW_BUF_SIZE   (64)

#define WORDS2BYTES(_priv, _wn)   ((_wn) * ((_priv)->nbits / 8))
#define BYTES2WORDS(_priv, _bn)   ((_bn) / ((_priv)->nbits / 8))

#define setbits(bs, a)     modifyreg32(a, 0, bs)
#define resetbits(bs, a)   modifyreg32(a, bs, 0)

/* SPI Slave controller hardware configuration */

struct spislave_config_s
{
  int32_t width;              /* SPI Slave default width */
  enum spi_slave_mode_e mode; /* SPI Slave default mode */

  uint8_t id;                 /* SPI device ID: SPIx {2,3} */
  uint8_t cs_pin;             /* GPIO configuration for CS */
  uint8_t mosi_pin;           /* GPIO configuration for MOSI */
  uint8_t miso_pin;           /* GPIO configuration for MISO */
  uint8_t clk_pin;            /* GPIO configuration for CLK */
#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  uint8_t io2_pin;            /* GPIO configuration for IO2 */
  uint8_t io3_pin;            /* GPIO configuration for IO3 */
#endif
  uint8_t periph;             /* Peripheral ID */
  uint8_t irq;                /* Interrupt ID */
  uint32_t clk_bit;           /* Clock enable bit */
  uint32_t rst_bit;           /* SPI reset bit */
#ifdef CONFIG_ESP32S3_SPI_DMA
  uint32_t dma_clk_bit;       /* DMA clock enable bit */
  uint32_t dma_rst_bit;       /* DMA reset bit */
  uint8_t dma_periph;         /* DMA peripheral */
#endif
  uint32_t cs_insig;          /* SPI CS input signal index */
  uint32_t cs_outsig;         /* SPI CS output signal index */
  uint32_t mosi_insig;        /* SPI MOSI input signal index */
  uint32_t mosi_outsig;       /* SPI MOSI output signal index */
  uint32_t miso_insig;        /* SPI MISO input signal index */
  uint32_t miso_outsig;       /* SPI MISO output signal index */
  uint32_t clk_insig;         /* SPI CLK input signal index */
  uint32_t clk_outsig;        /* SPI CLK output signal index */
#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  uint32_t io2_insig;
  uint32_t io2_outsig;
  uint32_t io3_insig;
  uint32_t io3_outsig;
#endif
};

struct spislave_priv_s
{
  /* Externally visible part of the SPI Slave controller interface */

  struct spi_slave_ctrlr_s ctrlr;

  /* Reference to SPI Slave device interface */

  struct spi_slave_dev_s *dev;

  /* Port configuration */

  const struct spislave_config_s *config;
  int refs;                   /* Reference count */
  int cpu;                    /* CPU ID */
  int cpuint;                 /* SPI interrupt ID */
#ifdef CONFIG_ESP32S3_SPI_DMA
  int32_t dma_channel;        /* Channel assigned by the GDMA driver */

  /* DMA RX/TX description */

  struct esp32s3_dmadesc_s *dma_rxdesc;
  struct esp32s3_dmadesc_s *dma_txdesc;

  uint32_t rx_dma_offset;     /* Offset of DMA RX buffer */
#endif
  enum spi_slave_mode_e mode; /* Current SPI Slave hardware mode */
  uint8_t nbits;              /* Current configured bit width */
  uint32_t tx_length;         /* Location of next TX value */

  /* SPI Slave TX queue buffer */

  uint8_t tx_buffer[SPI_SLAVE_BUFSIZE];
  uint32_t rx_length;         /* Location of next RX value */

  /* SPI Slave RX queue buffer */

  uint8_t rx_buffer[SPI_SLAVE_BUFSIZE];

  /* Flag that indicates whether SPI Slave is currently processing */

  bool is_processing;

  /* Flag that indicates whether SPI Slave TX is currently enabled */

  bool is_tx_enabled;

  spinlock_t lock;              /* Device specific lock. */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* SPI Slave controller interrupt handlers */

static int spislave_cs_interrupt(int irq, void *context, void *arg);
static int spislave_periph_interrupt(int irq, void *context, void *arg);

/* SPI Slave controller internal functions */

static void spislave_setmode(struct spi_slave_ctrlr_s *ctrlr,
                             enum spi_slave_mode_e mode);
static void spislave_setbits(struct spi_slave_ctrlr_s *ctrlr, int nbits);
static void spislave_store_result(struct spislave_priv_s *priv,
                                  uint32_t recv_bytes);
static void spislave_evict_sent_data(struct spislave_priv_s *priv,
                                     uint32_t sent_bytes);
#ifdef CONFIG_ESP32S3_SPI_DMA
static void spislave_setup_rx_dma(struct spislave_priv_s *priv);
static void spislave_setup_tx_dma(struct spislave_priv_s *priv);
static void spislave_prepare_next_rx(struct spislave_priv_s *priv);
static void spislave_prepare_next_tx(struct spislave_priv_s *priv);
#else
static void spislave_write_tx_buffer(struct spislave_priv_s *priv);
#endif
static int spislave_initialize(struct spi_slave_ctrlr_s *ctrlr);
static void spislave_deinitialize(struct spi_slave_ctrlr_s *ctrlr);

/* SPI Slave controller operations */

static void spislave_bind(struct spi_slave_ctrlr_s *ctrlr,
                          struct spi_slave_dev_s *dev,
                          enum spi_slave_mode_e mode,
                          int nbits);
static void spislave_unbind(struct spi_slave_ctrlr_s *ctrlr);
static int spislave_enqueue(struct spi_slave_ctrlr_s *ctrlr,
                            const void *data,
                            size_t nwords);
static bool spislave_qfull(struct spi_slave_ctrlr_s *ctrlr);
static void spislave_qflush(struct spi_slave_ctrlr_s *ctrlr);
static size_t spislave_qpoll(struct spi_slave_ctrlr_s *ctrlr);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/*  SPI2 private data */

#ifdef CONFIG_ESP32S3_SPI2
static const struct spislave_config_s esp32s3_spi2slave_config =
{
  .width        = SPI_SLAVE_DEFAULT_WIDTH,
  .mode         = SPI_SLAVE_DEFAULT_MODE,
  .id           = 2,
  .cs_pin       = CONFIG_ESP32S3_SPI2_CSPIN,
  .mosi_pin     = CONFIG_ESP32S3_SPI2_MOSIPIN,
  .miso_pin     = CONFIG_ESP32S3_SPI2_MISOPIN,
  .clk_pin      = CONFIG_ESP32S3_SPI2_CLKPIN,
#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  .io2_pin      = CONFIG_ESP32S3_SPI2_IO2PIN,
  .io3_pin      = CONFIG_ESP32S3_SPI2_IO3PIN,
#endif
  .periph       = ESP32S3_PERIPH_SPI2,
  .irq          = ESP32S3_IRQ_SPI2,
  .clk_bit      = SYSTEM_SPI2_CLK_EN,
  .rst_bit      = SYSTEM_SPI2_RST,
#ifdef CONFIG_ESP32S3_SPI_DMA
  .dma_clk_bit  = SYSTEM_SPI2_DMA_CLK_EN,
  .dma_rst_bit  = SYSTEM_SPI2_DMA_RST,
  .dma_periph   = ESP32S3_DMA_PERIPH_SPI2,
#endif
  .cs_insig     = FSPICS0_IN_IDX,
  .cs_outsig    = FSPICS0_OUT_IDX,
  .mosi_insig   = FSPID_IN_IDX,
  .mosi_outsig  = FSPID_OUT_IDX,
  .miso_insig   = FSPIQ_IN_IDX,
  .miso_outsig  = FSPIQ_OUT_IDX,
  .clk_insig    = FSPICLK_IN_IDX,
  .clk_outsig   = FSPICLK_OUT_IDX,
#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  .io2_insig    = FSPIWP_IN_IDX,
  .io2_outsig   = FSPIWP_OUT_IDX,
  .io3_insig    = FSPIHD_IN_IDX,
  .io3_outsig   = FSPIHD_OUT_IDX,
#endif
};

static const struct spi_slave_ctrlrops_s esp32s3_spi2slave_ops =
{
  .bind     = spislave_bind,
  .unbind   = spislave_unbind,
  .enqueue  = spislave_enqueue,
  .qfull    = spislave_qfull,
  .qflush   = spislave_qflush,
  .qpoll    = spislave_qpoll
};

#ifdef CONFIG_ESP32S3_SPI_DMA

/* SPI DMA RX/TX description buffer */

static struct esp32s3_dmadesc_s esp32s3_spi2_dma_rxdesc[SPI_DMA_DESC_NUM];
static struct esp32s3_dmadesc_s esp32s3_spi2_dma_txdesc[SPI_DMA_DESC_NUM];
#endif

static struct spislave_priv_s esp32s3_spi2slave_priv =
{
  .ctrlr         =
                  {
                    .ops = &esp32s3_spi2slave_ops
                  },
  .dev           = NULL,
  .config        = &esp32s3_spi2slave_config,
  .refs          = 0,
  .cpu           = -1,
  .cpuint        = -ENOMEM,
#ifdef CONFIG_ESP32S3_SPI_DMA
  .dma_channel   = -ENOMEM,
  .dma_rxdesc    = esp32s3_spi2_dma_rxdesc,
  .dma_txdesc    = esp32s3_spi2_dma_txdesc,
  .rx_dma_offset = 0,
#endif
  .mode          = SPISLAVE_MODE0,
  .nbits         = 0,
  .tx_length     = 0,
  .tx_buffer     =
                  {
                    0
                  },
  .rx_length     = 0,
  .rx_buffer     =
                  {
                    0
                  },
  .is_processing = false,
  .is_tx_enabled = false
};
#endif /* CONFIG_ESP32S3_SPI2 */

#ifdef CONFIG_ESP32S3_SPI3
static const struct spislave_config_s esp32s3_spi3slave_config =
{
  .width        = SPI_SLAVE_DEFAULT_WIDTH,
  .mode         = SPI_SLAVE_DEFAULT_MODE,
  .id           = 3,
  .cs_pin       = CONFIG_ESP32S3_SPI3_CSPIN,
  .mosi_pin     = CONFIG_ESP32S3_SPI3_MOSIPIN,
  .miso_pin     = CONFIG_ESP32S3_SPI3_MISOPIN,
  .clk_pin      = CONFIG_ESP32S3_SPI3_CLKPIN,
#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  .io2_pin      = CONFIG_ESP32S3_SPI3_IO2PIN,
  .io3_pin      = CONFIG_ESP32S3_SPI3_IO3PIN,
#endif
  .periph       = ESP32S3_PERIPH_SPI3,
  .irq          = ESP32S3_IRQ_SPI3,
  .clk_bit      = SYSTEM_SPI3_CLK_EN,
  .rst_bit      = SYSTEM_SPI3_RST,
#ifdef CONFIG_ESP32S3_SPI_DMA
  .dma_clk_bit  = SYSTEM_SPI3_DMA_CLK_EN,
  .dma_rst_bit  = SYSTEM_SPI3_DMA_RST,
  .dma_periph   = ESP32S3_DMA_PERIPH_SPI3,
#endif
  .cs_insig     = SPI3_CS0_IN_IDX,
  .cs_outsig    = SPI3_CS0_OUT_IDX,
  .mosi_insig   = SPI3_D_IN_IDX,
  .mosi_outsig  = SPI3_D_OUT_IDX,
  .miso_insig   = SPI3_Q_IN_IDX,
  .miso_outsig  = SPI3_Q_OUT_IDX,
  .clk_insig    = SPI3_CLK_IN_IDX,
  .clk_outsig   = SPI3_CLK_OUT_IDX,
#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  .io2_insig    = SPI3_WP_IN_IDX,
  .io2_outsig   = SPI3_WP_OUT_IDX,
  .io3_insig    = SPI3_HD_IN_IDX,
  .io3_outsig   = SPI3_HD_OUT_IDX,
#endif
};

static const struct spi_slave_ctrlrops_s esp32s3_spi3slave_ops =
{
  .bind     = spislave_bind,
  .unbind   = spislave_unbind,
  .enqueue  = spislave_enqueue,
  .qfull    = spislave_qfull,
  .qflush   = spislave_qflush,
  .qpoll    = spislave_qpoll
};

#ifdef CONFIG_ESP32S3_SPI_DMA

/* SPI DMA RX/TX description buffer */

static struct esp32s3_dmadesc_s esp32s3_spi3_dma_rxdesc[SPI_DMA_DESC_NUM];
static struct esp32s3_dmadesc_s esp32s3_spi3_dma_txdesc[SPI_DMA_DESC_NUM];
#endif

static struct spislave_priv_s esp32s3_spi3slave_priv =
{
  .ctrlr         =
                  {
                    .ops = &esp32s3_spi3slave_ops
                  },
  .dev           = NULL,
  .config        = &esp32s3_spi3slave_config,
  .refs          = 0,
  .cpu           = -1,
  .cpuint        = -ENOMEM,
#ifdef CONFIG_ESP32S3_SPI_DMA
  .dma_channel   = -ENOMEM,
  .dma_rxdesc    = esp32s3_spi3_dma_rxdesc,
  .dma_txdesc    = esp32s3_spi3_dma_txdesc,
  .rx_dma_offset = 0,
#endif
  .mode          = SPISLAVE_MODE0,
  .nbits         = 0,
  .tx_length     = 0,
  .tx_buffer     =
                  {
                    0
                  },
  .rx_length     = 0,
  .rx_buffer     =
                  {
                    0
                  },
  .is_processing = false,
  .is_tx_enabled = false
};
#endif /* CONFIG_ESP32S3_SPI3 */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: spislave_peripheral_reset
 *
 * Description:
 *   Reset the SPI Slave peripheral before next transaction.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static inline void spislave_peripheral_reset(struct spislave_priv_s *priv)
{
  setbits(SPI_SOFT_RESET_M, SPI_SLAVE_REG(priv->config->id));
  resetbits(SPI_SOFT_RESET_M, SPI_SLAVE_REG(priv->config->id));
}

/****************************************************************************
 * Name: spislave_cpu_tx_fifo_reset
 *
 * Description:
 *   Reset the BUF TX AFIFO, which is used to send data out in SPI Slave
 *   CPU-controlled mode transfer.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static inline void spislave_cpu_tx_fifo_reset(struct spislave_priv_s *priv)
{
  setbits(SPI_BUF_AFIFO_RST_M, SPI_DMA_CONF_REG(priv->config->id));
  resetbits(SPI_BUF_AFIFO_RST_M, SPI_DMA_CONF_REG(priv->config->id));
}

/****************************************************************************
 * Name: spislave_dma_tx_fifo_reset
 *
 * Description:
 *   Reset the DMA TX AFIFO, which is used to send data out in SPI Slave
 *   DMA-controlled mode transfer.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32S3_SPI_DMA
static inline void spislave_dma_tx_fifo_reset(struct spislave_priv_s *priv)
{
  setbits(SPI_DMA_AFIFO_RST_M, SPI_DMA_CONF_REG(priv->config->id));
  resetbits(SPI_DMA_AFIFO_RST_M, SPI_DMA_CONF_REG(priv->config->id));
}
#endif

/****************************************************************************
 * Name: spislave_dma_rx_fifo_reset
 *
 * Description:
 *   Reset the RX AFIFO, which is used to receive data in SPI Slave mode
 *   transfer.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32S3_SPI_DMA
static inline void spislave_dma_rx_fifo_reset(struct spislave_priv_s *priv)
{
  setbits(SPI_RX_AFIFO_RST_M, SPI_DMA_CONF_REG(priv->config->id));
  resetbits(SPI_RX_AFIFO_RST_M, SPI_DMA_CONF_REG(priv->config->id));
}
#endif

/****************************************************************************
 * Name: spislave_setmode
 *
 * Description:
 *   Set the SPI Slave mode.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *   mode  - Requested SPI Slave mode
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void spislave_setmode(struct spi_slave_ctrlr_s *ctrlr,
                             enum spi_slave_mode_e mode)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;

  spiinfo("mode=%d\n", mode);

  /* Has the mode changed? */

  if (mode != priv->mode)
    {
      uint32_t ck_idle_edge;
      uint32_t rsck_i_edge;
      uint32_t tsck_i_edge;
      uint32_t clk_mode_13;

      switch (mode)
        {
          case SPISLAVE_MODE0: /* CPOL=0; CPHA=0 */
            ck_idle_edge = 0;
            rsck_i_edge = 0;
            tsck_i_edge = 0;
            clk_mode_13 = 0;
            break;

          case SPISLAVE_MODE1: /* CPOL=0; CPHA=1 */
            ck_idle_edge = 0;
            rsck_i_edge = 1;
            tsck_i_edge = 1;
            clk_mode_13 = 1;
            break;

          case SPISLAVE_MODE2: /* CPOL=1; CPHA=0 */
            ck_idle_edge = 1;
            rsck_i_edge = 1;
            tsck_i_edge = 1;
            clk_mode_13 = 0;
            break;

          case SPISLAVE_MODE3: /* CPOL=1; CPHA=1 */
            ck_idle_edge = 1;
            rsck_i_edge = 0;
            tsck_i_edge = 0;
            clk_mode_13 = 1;
            break;

          default:
            spierr("Invalid mode: %d\n", mode);
            DEBUGPANIC();
            return;
        }

      modifyreg32(SPI_MISC_REG(priv->config->id),
                  SPI_CK_IDLE_EDGE_M,
                  VALUE_TO_FIELD(ck_idle_edge, SPI_CK_IDLE_EDGE));

      modifyreg32(SPI_USER_REG(priv->config->id),
                  SPI_RSCK_I_EDGE_M | SPI_TSCK_I_EDGE_M,
                  VALUE_TO_FIELD(rsck_i_edge, SPI_RSCK_I_EDGE) |
                  VALUE_TO_FIELD(tsck_i_edge, SPI_TSCK_I_EDGE));

      modifyreg32(SPI_SLAVE_REG(priv->config->id),
                  SPI_CLK_MODE_13_M | SPI_RSCK_DATA_OUT_M,
                  VALUE_TO_FIELD(clk_mode_13, SPI_CLK_MODE_13));

      priv->mode = mode;
    }
}

/****************************************************************************
 * Name: spislave_setbits
 *
 * Description:
 *   Set the number of bits per word.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *   nbits - The number of bits in an SPI word
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void spislave_setbits(struct spi_slave_ctrlr_s *ctrlr, int nbits)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;

  spiinfo("nbits=%d\n", nbits);

  priv->nbits = nbits;
}

/****************************************************************************
 * Name: spislave_cs_interrupt
 *
 * Description:
 *   Handler for the GPIO interrupt which is triggered when the chip select
 *   has toggled to inactive state (active high).
 *
 * Input Parameters:
 *   irq     - Number of the IRQ that generated the interrupt
 *   context - Interrupt register state save info
 *   arg     - SPI Slave controller private data
 *
 * Returned Value:
 *   Standard interrupt return value.
 *
 ****************************************************************************/

static int spislave_cs_interrupt(int irq, void *context, void *arg)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)arg;

  if (priv->is_processing)
    {
      priv->is_processing = false;
      SPIS_DEV_SELECT(priv->dev, false);
    }

  return 0;
}

/****************************************************************************
 * Name: spislave_store_result
 *
 * Description:
 *   Fetch data from the SPI hardware data buffer and record the length.
 *   This is a post transaction operation.
 *
 * Input Parameters:
 *   priv       - Private SPI Slave controller structure
 *   recv_bytes - Number of received bytes
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void spislave_store_result(struct spislave_priv_s *priv,
                                  uint32_t recv_bytes)
{
  uint32_t remaining_space = SPI_SLAVE_BUFSIZE - priv->rx_length;
  uint32_t bytes_to_copy = recv_bytes;

  if (bytes_to_copy > remaining_space)
    {
      spiwarn("RX buffer full! Discarded %" PRIu32 " received bytes\n",
              bytes_to_copy - remaining_space);

      bytes_to_copy = remaining_space;
    }

#ifdef CONFIG_ESP32S3_SPI_DMA
  if (bytes_to_copy)
    {
      if ((priv->rx_dma_offset != priv->rx_length))
        {
          memmove(priv->rx_buffer + priv->rx_length,
                  priv->rx_buffer + priv->rx_dma_offset,
                  bytes_to_copy);

          priv->rx_dma_offset = priv->rx_length;
        }

      priv->rx_length += bytes_to_copy;
    }
#else
  /* If DMA is not enabled, software should copy incoming data from data
   * buffer registers to receive buffer.
   */

  if (bytes_to_copy)
    {
      /* Set data_buf_reg with the address of the first data buffer
       * register (W0).
       */

      uintptr_t data_buf_reg = SPI_W0_REG(priv->config->id);

      /* Read received data words from SPI hardware data buffer. */

      for (int i = 0; i < bytes_to_copy; i += sizeof(uint32_t))
        {
          uint32_t rbytes = MIN(bytes_to_copy - i, sizeof(uintptr_t));
          uint32_t r_wd = getreg32(data_buf_reg);

          memcpy(priv->rx_buffer + priv->rx_length + i, &r_wd, rbytes);

          /* Update data_buf_reg to point to the next data buffer register. */

          data_buf_reg += sizeof(uintptr_t);
        }

      priv->rx_length += bytes_to_copy;
    }
#endif
}

/****************************************************************************
 * Name: spislave_prepare_next_rx
 *
 * Description:
 *   Prepare the SPI Slave controller for receiving data on the next
 *   transaction.
 *
 * Input Parameters:
 *   priv   - Private SPI Slave controller structure
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32S3_SPI_DMA
static void spislave_prepare_next_rx(struct spislave_priv_s *priv)
{
  if (priv->rx_length < SPI_SLAVE_BUFSIZE)
    {
      spislave_setup_rx_dma(priv);
    }
}
#endif

/****************************************************************************
 * Name: spislave_evict_sent_data
 *
 * Description:
 *   Evict from the TX buffer data sent on the latest transaction and update
 *   the length. This is a post transaction operation.
 *
 * Input Parameters:
 *   priv       - Private SPI Slave controller structure
 *   sent_bytes - Number of transmitted bytes
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void spislave_evict_sent_data(struct spislave_priv_s *priv,
                                     uint32_t sent_bytes)
{
  if (sent_bytes < priv->tx_length)
    {
      priv->tx_length -= sent_bytes;

      memmove(priv->tx_buffer, priv->tx_buffer + sent_bytes,
              priv->tx_length);

      memset(priv->tx_buffer + priv->tx_length, 0, sent_bytes);
    }
  else
    {
      priv->tx_length = 0;
    }
}

/****************************************************************************
 * Name: spislave_write_tx_buffer
 *
 * Description:
 *   Write to SPI Slave peripheral hardware data buffer.
 *
 * Input Parameters:
 *   priv   - Private SPI Slave controller structure
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

#ifndef CONFIG_ESP32S3_SPI_DMA
static void spislave_write_tx_buffer(struct spislave_priv_s *priv)
{
  /* Initialize data_buf_reg with the address of the first data buffer
   * register (W0).
   */

  uintptr_t data_buf_reg = SPI_W0_REG(priv->config->id);

  uint32_t transfer_size = MIN(SPI_SLAVE_HW_BUF_SIZE, priv->tx_length);

  /* Write data words to hardware data buffer.
   * SPI peripheral contains 16 registers (W0 - W15).
   */

  for (int i = 0; i < transfer_size; i += sizeof(uint32_t))
    {
      uint32_t w_wd = UINT32_MAX;

      memcpy(&w_wd, priv->tx_buffer + i, sizeof(uint32_t));

      putreg32(w_wd, data_buf_reg);

      /* Update data_buf_reg to point to the next data buffer register. */

      data_buf_reg += sizeof(uintptr_t);
    }
}
#endif

/****************************************************************************
 * Name: spislave_setup_rx_dma
 *
 * Description:
 *   Configure the SPI Slave peripheral to perform the next RX data transfer
 *   via DMA.
 *
 * Input Parameters:
 *   priv   - Private SPI Slave controller structure
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32S3_SPI_DMA
static void spislave_setup_rx_dma(struct spislave_priv_s *priv)
{
  uint32_t length = SPI_SLAVE_BUFSIZE - priv->rx_length;

  esp32s3_dma_setup(priv->dma_rxdesc,
                    SPI_DMA_DESC_NUM,
                    priv->rx_buffer + priv->rx_length,
                    length,
                    false, priv->dma_channel);
  esp32s3_dma_load(priv->dma_rxdesc, priv->dma_channel, false);

  priv->rx_dma_offset = priv->rx_length;

  spislave_dma_rx_fifo_reset(priv);

  spislave_peripheral_reset(priv);

  /* Clear input FIFO full error */

  setbits(SPI_DMA_INFIFO_FULL_ERR_INT_CLR_M,
          SPI_DMA_INT_CLR_REG(priv->config->id));

  /* Enable SPI DMA RX */

  setbits(SPI_DMA_RX_ENA_M, SPI_DMA_CONF_REG(priv->config->id));

  esp32s3_dma_enable(priv->dma_channel, false);
}
#endif

/****************************************************************************
 * Name: spislave_setup_tx_dma
 *
 * Description:
 *   Configure the SPI Slave peripheral to perform the next TX data transfer
 *   via DMA.
 *
 * Input Parameters:
 *   priv   - Private SPI Slave controller structure
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32S3_SPI_DMA
static void spislave_setup_tx_dma(struct spislave_priv_s *priv)
{
  esp32s3_dma_setup(priv->dma_txdesc,
                    SPI_DMA_DESC_NUM,
                    priv->tx_buffer,
                    SPI_SLAVE_BUFSIZE,
                    true, priv->dma_channel);
  esp32s3_dma_load(priv->dma_txdesc, priv->dma_channel, true);

  spislave_dma_tx_fifo_reset(priv);

  spislave_peripheral_reset(priv);

  /* Clear output FIFO empty error */

  setbits(SPI_DMA_OUTFIFO_EMPTY_ERR_INT_CLR_M,
          SPI_DMA_INT_CLR_REG(priv->config->id));

  /* Enable SPI DMA TX */

  setbits(SPI_DMA_TX_ENA_M, SPI_DMA_CONF_REG(priv->config->id));

  esp32s3_dma_enable(priv->dma_channel, true);
}
#endif

/****************************************************************************
 * Name: spislave_prepare_next_tx
 *
 * Description:
 *   Prepare the SPI Slave controller for transmitting data on the next
 *   transaction.
 *
 * Input Parameters:
 *   priv   - Private SPI Slave controller structure
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void spislave_prepare_next_tx(struct spislave_priv_s *priv)
{
  if (priv->tx_length != 0)
    {
#ifdef CONFIG_ESP32S3_SPI_DMA
      spislave_setup_tx_dma(priv);
#else
      spislave_peripheral_reset(priv);

      spislave_write_tx_buffer(priv);

      spislave_cpu_tx_fifo_reset(priv);
#endif

      priv->is_tx_enabled = true;
    }
  else
    {
      spiwarn("TX buffer empty! Disabling TX for next transaction\n");

      spislave_cpu_tx_fifo_reset(priv);

      priv->is_tx_enabled = false;
    }
}

/****************************************************************************
 * Name: spislave_periph_interrupt
 *
 * Description:
 *   Handler for the SPI Slave controller interrupt which is triggered when a
 *   transfer is finished.
 *
 * Input Parameters:
 *   irq     - Number of the IRQ that generated the interrupt
 *   context - Interrupt register state save info
 *   arg     - SPI Slave controller private data
 *
 * Returned Value:
 *   Standard interrupt return value.
 *
 ****************************************************************************/

static int spislave_periph_interrupt(int irq, void *context, void *arg)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)arg;
  uint32_t regval = getreg32(SPI_SLAVE1_REG(priv->config->id));
  uint32_t transfer_size = REG_MASK(regval, SPI_SLV_DATA_BITLEN) / 8;

#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  uint32_t int_clear = 0;
  uint32_t int_status = getreg32(SPI_DMA_INT_ST_REG(priv->config->id));
#else
  uint32_t int_clear = SPI_TRANS_DONE_INT_CLR_M;
#endif

  if (!priv->is_processing)
    {
      SPIS_DEV_SELECT(priv->dev, true);
      priv->is_processing = true;
    }

  /* RX process */

#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  if (int_status & SPI_SLV_INT_RX)
    {
#endif
      if (transfer_size > 0)
        {
          spislave_store_result(priv, transfer_size);
          SPIS_DEV_NOTIFY(priv->dev, SPISLAVE_RX_COMPLETE);
        }

#ifdef CONFIG_ESP32S3_SPI_DMA
      spislave_prepare_next_rx(priv);
#endif

#ifdef CONFIG_ESP32S3_SPI_IO_QIO
      int_clear |= SPI_SLV_INT_CLR_RX;
    }
#endif

  /* TX process */

#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  if (int_status & SPI_SLV_INT_TX)
    {
#endif
      if (priv->is_tx_enabled && transfer_size > 0)
        {
          spislave_evict_sent_data(priv, transfer_size);
          SPIS_DEV_NOTIFY(priv->dev, SPISLAVE_TX_COMPLETE);
        }

      spislave_prepare_next_tx(priv);

#ifdef CONFIG_ESP32S3_SPI_IO_QIO
      int_clear |= SPI_SLV_INT_CLR_TX;
    }
#endif

  if (priv->is_processing && esp32s3_gpioread(priv->config->cs_pin))
    {
      priv->is_processing = false;
      SPIS_DEV_SELECT(priv->dev, false);
    }

  /* Clear the trans_done interrupt flag */

  setbits(int_clear, SPI_DMA_INT_CLR_REG(priv->config->id));

  /* Trigger the start of user-defined transaction */

#ifndef CONFIG_ESP32S3_SPI_IO_QIO
  setbits(SPI_USR_M, SPI_CMD_REG(priv->config->id));
#endif

  return 0;
}

/****************************************************************************
 * Name: spislave_dma_init
 *
 * Description:
 *   Initialize ESP32-S3 SPI Slave connection to GDMA engine.
 *
 * Input Parameters:
 *   priv   - Private SPI Slave controller structure
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32S3_SPI_DMA
static int spislave_dma_init(struct spislave_priv_s *priv)
{
  /* Enable GDMA clock for the SPI peripheral */

  setbits(priv->config->dma_clk_bit, SYSTEM_PERIP_CLK_EN0_REG);

  /* Reset GDMA for the SPI peripheral */

  resetbits(priv->config->dma_rst_bit, SYSTEM_PERIP_RST_EN0_REG);

  /* Request a GDMA channel for SPI peripheral */

  priv->dma_channel = esp32s3_dma_request(priv->config->dma_periph, 1, 1,
                                          true);
  if (priv->dma_channel < 0)
    {
      spierr("Failed to allocate GDMA channel\n");
      return ERROR;
    }

  /* Disable segment transaction mode for SPI Slave */

  resetbits(SPI_DMA_SLV_SEG_TRANS_EN_M, SPI_DMA_CONF_REG(priv->config->id));

  /* Configure DMA In-Link EOF to be generated by trans_done */

  resetbits(SPI_RX_EOF_EN_M, SPI_DMA_CONF_REG(priv->config->id));

  return OK;
}

/****************************************************************************
 * Name: spislave_dma_deinit
 *
 * Description:
 *   Deinitialize ESP32-S3 SPI slave GDMA engine.
 *
 * Input Parameters:
 *   dev - Device-specific state data
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

void spislave_dma_deinit(struct spislave_priv_s *priv)
{
  /* Release a DMA channel from peripheral */

  esp32s3_dma_release(priv->dma_channel);

  /* Disable DMA clock for the SPI peripheral */

  modifyreg32(SYSTEM_PERIP_CLK_EN0_REG, priv->config->dma_clk_bit, 0);
}
#endif

/****************************************************************************
 * Name: spislave_initializ_iomux
 *
 * Description:
 *   Initialize ESP32-S3 SPI Slave GPIO by IO MUX.
 *
 * Input Parameters:
 *   priv - Private SPI Slave controller structure
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

#if SPI_VIA_IOMUX != 0
static void spislave_initializ_iomux(struct spislave_priv_s *priv)
{
  uint32_t attr = INPUT_FUNCTION_5 | DRIVE_0;
  const struct spislave_config_s *config = priv->config;

  esp32s3_configgpio(config->cs_pin,  attr);
  esp32s3_configgpio(config->clk_pin, attr);

  esp32s3_gpio_matrix_out(config->cs_pin,   SIG_GPIO_OUT_IDX, 0, 0);
  esp32s3_gpio_matrix_out(config->clk_pin,  SIG_GPIO_OUT_IDX, 0, 0);
  esp32s3_gpio_matrix_out(config->mosi_pin, SIG_GPIO_OUT_IDX, 0, 0);
  esp32s3_gpio_matrix_out(config->miso_pin, SIG_GPIO_OUT_IDX, 0, 0);

#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  attr |= OUTPUT;

  esp32s3_configgpio(config->mosi_pin, attr);
  esp32s3_configgpio(config->miso_pin, attr);
  esp32s3_configgpio(config->io2_pin,  attr);
  esp32s3_configgpio(config->io3_pin,  attr);

  esp32s3_gpio_matrix_out(config->io2_pin, SIG_GPIO_OUT_IDX, 0, 0);
  esp32s3_gpio_matrix_out(config->io3_pin, SIG_GPIO_OUT_IDX, 0, 0);
#else
  esp32s3_configgpio(config->mosi_pin, attr);
  esp32s3_configgpio(config->miso_pin, OUTPUT_FUNCTION_5);
#endif
}
#endif

/****************************************************************************
 * Name: spislave_initializ_iomatrix
 *
 * Description:
 *   Initialize ESP32-S3 SPI Slave GPIO by IO matrix.
 *
 * Input Parameters:
 *   priv - Private SPI Slave controller structure
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

#if SPI_VIA_IOMUX == 0 || defined(CONFIG_ESP32S3_SPI3)
static void spislave_initializ_iomatrix(struct spislave_priv_s *priv)
{
  uint32_t attr = INPUT | DRIVE_0;
  const struct spislave_config_s *config = priv->config;

  esp32s3_configgpio(config->cs_pin, attr);
  esp32s3_gpio_matrix_in(config->cs_pin, config->cs_insig, 0);

  esp32s3_configgpio(config->clk_pin, attr);
  esp32s3_gpio_matrix_in(config->clk_pin, config->clk_insig, 0);

#  ifdef CONFIG_ESP32S3_SPI_IO_QIO
  attr |= OUTPUT;

  esp32s3_configgpio(config->mosi_pin, attr);
  esp32s3_gpio_matrix_in(config->mosi_pin, config->mosi_insig, 0);
  esp32s3_gpio_matrix_out(config->mosi_pin, config->mosi_outsig, 0, 0);

  esp32s3_configgpio(config->miso_pin, attr);
  esp32s3_gpio_matrix_in(config->miso_pin, config->miso_insig, 0);
  esp32s3_gpio_matrix_out(config->miso_pin, config->miso_outsig, 0, 0);

  esp32s3_configgpio(config->io2_pin, attr);
  esp32s3_gpio_matrix_in(config->io2_pin, config->io2_insig, 0);
  esp32s3_gpio_matrix_out(config->io2_pin, config->io2_outsig, 0, 0);

  esp32s3_configgpio(config->io3_pin, attr);
  esp32s3_gpio_matrix_in(config->io3_pin, config->io3_insig, 0);
  esp32s3_gpio_matrix_out(config->io3_pin, config->io3_outsig, 0, 0);
#  else
  esp32s3_configgpio(config->mosi_pin, attr);
  esp32s3_gpio_matrix_in(config->mosi_pin, config->mosi_insig, 0);

  esp32s3_configgpio(config->miso_pin, OUTPUT);
  esp32s3_gpio_matrix_out(config->miso_pin, config->miso_outsig, 0, 0);
#  endif
}
#endif

/****************************************************************************
 * Name: spislave_gpio_initialize
 *
 * Description:
 *   Initialize ESP32-S3 SPI Slave GPIO
 *
 * Input Parameters:
 *   priv - Private SPI Slave controller structure
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void spislave_gpio_initialize(struct spislave_priv_s *priv)
{
#if SPI_VIA_IOMUX != 0
  if (priv->config->id == 2)
    {
      spislave_initializ_iomux(priv);
    }
#  ifdef CONFIG_ESP32S3_SPI3
  else
    {
      spislave_initializ_iomatrix(priv);
    }
#  endif
#else
  spislave_initializ_iomatrix(priv);
#endif
}

/****************************************************************************
 * Name: spislave_initialize
 *
 * Description:
 *   Initialize ESP32-S3 SPI Slave hardware interface.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   Zero on success; a negated errno on failure
 *
 ****************************************************************************/

static int spislave_initialize(struct spi_slave_ctrlr_s *ctrlr)
{
  uint32_t regval;
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  const struct spislave_config_s *config = priv->config;

  spiinfo("ctrlr=%p\n", ctrlr);

  spislave_gpio_initialize(priv);

  setbits(config->clk_bit, SYSTEM_PERIP_CLK_EN0_REG);
  resetbits(config->rst_bit, SYSTEM_PERIP_RST_EN0_REG);

  /* Configure SPI Slave peripheral */

  putreg32(0, SPI_CLOCK_REG(priv->config->id));

#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  regval = 0;
#else
  regval = SPI_DOUTDIN_M;
#endif
  putreg32(regval, SPI_USER_REG(priv->config->id));

  putreg32(0, SPI_CTRL_REG(priv->config->id));

  regval = SPI_SLAVE_MODE_M;
#ifdef CONFIG_ESP32S3_SPI_DMA
  regval |= SPI_SLV_WRDMA_BITLEN_EN_M | SPI_SLV_RDDMA_BITLEN_EN_M;
#else
  regval |= SPI_SLV_WRBUF_BITLEN_EN_M | SPI_SLV_RDBUF_BITLEN_EN_M;
#endif

  putreg32(regval, SPI_SLAVE_REG(priv->config->id));

  spislave_peripheral_reset(priv);

  /* Use all 64 bytes of the SPI hardware data buffer */

  resetbits(SPI_USR_MISO_HIGHPART_M | SPI_USR_MOSI_HIGHPART_M,
            SPI_USER_REG(priv->config->id));

  /* Disable interrupts */

  resetbits(SPI_INT_MASK, SPI_DMA_INT_ENA_REG(priv->config->id));

#ifdef CONFIG_ESP32S3_SPI_DMA
  if (spislave_dma_init(priv) != OK)
    {
      setbits(priv->config->clk_bit, SYSTEM_PERIP_RST_EN0_REG);
      resetbits(priv->config->clk_bit, SYSTEM_PERIP_CLK_EN0_REG);
      return ERROR;
    }
#endif

  esp32s3_gpioirqenable(ESP32S3_PIN2IRQ(config->cs_pin), RISING);

  /* Force a transaction done interrupt.
   * This interrupt won't fire yet because we initialized the SPI interrupt
   * as disabled. This way, we can just enable the SPI interrupt and the
   * interrupt handler will kick in, handling any transactions that are
   * queued.
   */

#ifdef CONFIG_ESP32S3_SPI_IO_QIO
  regval = SPI_SLV_INT_EN;
#else
  regval = SPI_TRANS_DONE_INT_ENA_M;
#endif
  setbits(regval, SPI_DMA_INT_RAW_REG(priv->config->id));
  setbits(regval, SPI_DMA_INT_ENA_REG(priv->config->id));

  return OK;
}

/****************************************************************************
 * Name: spislave_deinitialize
 *
 * Description:
 *   Deinitialize ESP32-S3 SPI Slave hardware interface.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void spislave_deinitialize(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;

  esp32s3_gpioirqdisable(ESP32S3_PIN2IRQ(priv->config->cs_pin));

  /* Disable the trans_done interrupt */

  resetbits(SPI_TRANS_DONE_INT_ENA_M, SPI_DMA_INT_ENA_REG(priv->config->id));

#ifdef CONFIG_ESP32S3_SPI_DMA
  spislave_dma_deinit(priv);
  priv->rx_dma_offset = 0;
#endif

  setbits(priv->config->clk_bit, SYSTEM_PERIP_RST_EN0_REG);
  resetbits(priv->config->clk_bit, SYSTEM_PERIP_CLK_EN0_REG);

  priv->mode = SPISLAVE_MODE0;
  priv->nbits = 0;
  priv->tx_length = 0;
  priv->rx_length = 0;
  priv->is_processing = false;
  priv->is_tx_enabled = false;
}

/****************************************************************************
 * Name: spislave_bind
 *
 * Description:
 *   Bind the SPI Slave device interface to the SPI Slave controller
 *   interface and configure the SPI interface. Upon return, the SPI
 *   slave controller driver is fully operational and ready to perform
 *   transfers.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *   dev   - SPI Slave device interface instance
 *   mode  - The SPI mode requested
 *   nbits - The number of bits requests.
 *            If value is greater than 0, then it implies MSB first
 *            If value is less than 0, then it implies LSB first with -nbits
 *
 * Returned Value:
 *   None.
 *
 * Assumptions:
 *   This implementation currently supports only positive "nbits" values,
 *   i.e., it always configures the SPI Slave controller driver as MSB first.
 *
 ****************************************************************************/

static void spislave_bind(struct spi_slave_ctrlr_s *ctrlr,
                          struct spi_slave_dev_s *dev,
                          enum spi_slave_mode_e mode,
                          int nbits)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  const void *data = NULL;
  irqstate_t flags;
  size_t num_words;

  spiinfo("ctrlr=%p dev=%p mode=%d nbits=%d\n", ctrlr, dev, mode, nbits);

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev == NULL);
  DEBUGASSERT(dev != NULL);
  DEBUGASSERT(nbits > 0);

  flags = spin_lock_irqsave(&priv->lock);

  priv->dev = dev;

  SPIS_DEV_SELECT(dev, false);

  SPIS_DEV_CMDDATA(dev, false);

#ifdef CONFIG_ESP32S3_SPI_DMA
  priv->rx_dma_offset = 0;
#endif

  priv->rx_length = 0;
  priv->tx_length = 0;
  priv->is_tx_enabled = false;

  if (spislave_initialize(ctrlr) != OK)
    {
      spierr("spislave_initialize failed!\n");
      spin_unlock_irqrestore(&priv->lock, flags);
      DEBUGPANIC();
    }

  spislave_setmode(ctrlr, mode);
  spislave_setbits(ctrlr, nbits);

  num_words = SPIS_DEV_GETDATA(dev, &data);

  if (data != NULL && num_words > 0)
    {
      size_t num_bytes = WORDS2BYTES(priv, num_words);
      memcpy(priv->tx_buffer, data, num_bytes);
      priv->tx_length += num_bytes;
    }

#ifdef CONFIG_ESP32S3_SPI_DMA
  spislave_prepare_next_rx(priv);
#endif

  /* Enable the CPU interrupt that is linked to the SPI Slave controller */

  up_enable_irq(priv->config->irq);

  spin_unlock_irqrestore(&priv->lock, flags);
}

/****************************************************************************
 * Name: spislave_unbind
 *
 * Description:
 *   Un-bind the SPI Slave device interface from the SPI Slave controller
 *   interface. Reset the SPI interface and restore the SPI Slave
 *   controller driver to its initial state.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void spislave_unbind(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  irqstate_t flags;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev != NULL);

  spiinfo("Unbinding %p\n", priv->dev);

  flags = spin_lock_irqsave(&priv->lock);

  up_disable_irq(priv->config->irq);

  esp32s3_gpioirqdisable(ESP32S3_PIN2IRQ(priv->config->cs_pin));

  /* Disable the trans_done interrupt */

  resetbits(SPI_TRANS_DONE_INT_ENA_M, SPI_DMA_INT_ENA_REG(priv->config->id));

#ifdef CONFIG_ESP32S3_SPI_DMA
  resetbits(priv->config->dma_clk_bit, SYSTEM_PERIP_CLK_EN0_REG);
#endif

  resetbits(priv->config->clk_bit, SYSTEM_PERIP_CLK_EN0_REG);

  priv->dev = NULL;

  spin_unlock_irqrestore(&priv->lock, flags);
}

/****************************************************************************
 * Name: spislave_enqueue
 *
 * Description:
 *   Enqueue the next value to be shifted out from the interface. This adds
 *   the word to the controller driver for a subsequent transfer but has no
 *   effect on any in-process or currently "committed" transfers.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *   data  - Pointer to the command/data mode data to be shifted out.
 *           The data width must be aligned to the nbits parameter which was
 *           previously provided to the bind() method.
 *   len   - Number of units of "nbits" wide to enqueue,
 *           "nbits" being the data width previously provided to the bind()
 *           method.
 *
 * Returned Value:
 *   Number of data items successfully queued, or a negated errno:
 *         - "len" if all the data was successfully queued
 *         - "0..len-1" if queue is full
 *         - "-errno" in any other error
 *
 ****************************************************************************/

static int spislave_enqueue(struct spi_slave_ctrlr_s *ctrlr,
                            const void *data,
                            size_t len)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  size_t num_bytes = WORDS2BYTES(priv, len);
  size_t bufsize;
  irqstate_t flags;
  int enqueued_words;

  spiinfo("ctrlr=%p, data=%p, num_bytes=%zu\n", ctrlr, data, num_bytes);

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev != NULL);

  flags = spin_lock_irqsave(&priv->lock);

  bufsize = SPI_SLAVE_BUFSIZE - priv->tx_length;
  if (bufsize == 0)
    {
      spin_unlock_irqrestore(&priv->lock, flags);
      return -ENOSPC;
    }

  num_bytes = MIN(num_bytes, bufsize);
  memcpy(priv->tx_buffer + priv->tx_length, data, num_bytes);
  priv->tx_length += num_bytes;

  enqueued_words = BYTES2WORDS(priv, num_bytes);

  if (!priv->is_processing)
    {
      spislave_prepare_next_tx(priv);
    }

  spin_unlock_irqrestore(&priv->lock, flags);

  return enqueued_words;
}

/****************************************************************************
 * Name: spislave_qfull
 *
 * Description:
 *   Return true if the queue is full or false if there is space to add an
 *   additional word to the queue.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   true if the output queue is full, false otherwise.
 *
 ****************************************************************************/

static bool spislave_qfull(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  irqstate_t flags;
  bool is_full;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev != NULL);

  spiinfo("ctrlr=%p\n", ctrlr);

  flags = spin_lock_irqsave(&priv->lock);
  is_full = priv->tx_length == SPI_SLAVE_BUFSIZE;
  spin_unlock_irqrestore(&priv->lock, flags);

  return is_full;
}

/****************************************************************************
 * Name: spislave_qflush
 *
 * Description:
 *   Discard all saved values in the output queue. On return from this
 *   function the output queue will be empty. Any in-progress or otherwise
 *   "committed" output values may not be flushed.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void spislave_qflush(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  irqstate_t flags;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev != NULL);

  spiinfo("ctrlr=%p\n", ctrlr);

  flags = spin_lock_irqsave(&priv->lock);
  priv->tx_length = 0;
  priv->is_tx_enabled = false;
  spin_unlock_irqrestore(&priv->lock, flags);
}

/****************************************************************************
 * Name: spislave_qpoll
 *
 * Description:
 *   Tell the controller to output all the receive queue data.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   Number of units of width "nbits" left in the RX queue. If the device
 *   accepted all the data, the return value will be 0.
 *
 ****************************************************************************/

static size_t spislave_qpoll(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  irqstate_t flags;
  uint32_t tmp;
  uint32_t recv_n;
  size_t remaining_words;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->dev != NULL);

  spiinfo("ctrlr=%p\n", ctrlr);

  flags = spin_lock_irqsave(&priv->lock);

  tmp = SPIS_DEV_RECEIVE(priv->dev, priv->rx_buffer,
                         BYTES2WORDS(priv, priv->rx_length));
  if (tmp)
    {
      recv_n = WORDS2BYTES(priv, tmp);
      if (recv_n < priv->rx_length)
        {
          /* If the upper layer does not receive all of the data from the
           * receive buffer, move the remaining data to the head of the
           * buffer.
           */

          priv->rx_length -= recv_n;
          memmove(priv->rx_buffer, priv->rx_buffer + recv_n,
                  priv->rx_length);
        }
      else
        {
          priv->rx_length = 0;
        }

  #ifdef CONFIG_ESP32S3_SPI_DMA
      spislave_setup_rx_dma(priv);
  #endif
    }

  remaining_words = BYTES2WORDS(priv, priv->rx_length);

  spin_unlock_irqrestore(&priv->lock, flags);

  return remaining_words;
}

/****************************************************************************
 * Name: esp32s3_spislave_ctrlr_initialize
 *
 * Description:
 *   Initialize the selected SPI Slave bus.
 *
 * Input Parameters:
 *   port - Port number (for hardware that has multiple SPI Slave interfaces)
 *
 * Returned Value:
 *   Valid SPI Slave controller structure reference on success;
 *   NULL on failure.
 *
 ****************************************************************************/

struct spi_slave_ctrlr_s *esp32s3_spislave_ctrlr_initialize(int port)
{
  struct spi_slave_ctrlr_s *spislave_dev;
  struct spislave_priv_s *priv;
  irqstate_t flags;

  switch (port)
    {
#ifdef CONFIG_ESP32S3_SPI2
      case ESP32S3_SPI2:
        priv = &esp32s3_spi2slave_priv;
        break;
#endif
#ifdef CONFIG_ESP32S3_SPI3
      case ESP32S3_SPI3:
        priv = &esp32s3_spi3slave_priv;
        break;
#endif
      default:
        return NULL;
    }

  spislave_dev = (struct spi_slave_ctrlr_s *)priv;

  flags = spin_lock_irqsave(&priv->lock);

  if ((volatile int)priv->refs != 0)
    {
      spin_unlock_irqrestore(&priv->lock, flags);

      return spislave_dev;
    }

  /* Attach IRQ for CS pin interrupt */

  DEBUGVERIFY(irq_attach(ESP32S3_PIN2IRQ(priv->config->cs_pin),
                         spislave_cs_interrupt,
                         priv));

  priv->cpu = this_cpu();
  priv->cpuint = esp32s3_setup_irq(priv->cpu,
                                   priv->config->periph,
                                   ESP32S3_INT_PRIO_DEF,
                                   ESP32S3_CPUINT_LEVEL);
  if (priv->cpuint < 0)
    {
      /* Failed to allocate a CPU interrupt of this type. */

      spin_unlock_irqrestore(&priv->lock, flags);

      return NULL;
    }

  if (irq_attach(priv->config->irq, spislave_periph_interrupt, priv) != OK)
    {
      /* Failed to attach IRQ, so CPU interrupt must be freed. */

      esp32s3_teardown_irq(priv->cpu, priv->config->periph, priv->cpuint);
      spin_unlock_irqrestore(&priv->lock, flags);

      return NULL;
    }

  priv->refs++;

  spin_unlock_irqrestore(&priv->lock, flags);

  return spislave_dev;
}

/****************************************************************************
 * Name: esp32s3_spislave_ctrlr_uninitialize
 *
 * Description:
 *   Uninitialize an SPI Slave bus.
 *
 * Input Parameters:
 *   ctrlr - SPI Slave controller interface instance
 *
 * Returned Value:
 *   Zero (OK) is returned on success. Otherwise -1 (ERROR).
 *
 ****************************************************************************/

int esp32s3_spislave_ctrlr_uninitialize(struct spi_slave_ctrlr_s *ctrlr)
{
  struct spislave_priv_s *priv = (struct spislave_priv_s *)ctrlr;
  irqstate_t flags;

  DEBUGASSERT(ctrlr != NULL);

  if (priv->refs == 0)
    {
      return ERROR;
    }

  flags = spin_lock_irqsave(&priv->lock);

  if (--priv->refs)
    {
      spin_unlock_irqrestore(&priv->lock, flags);
      return OK;
    }

  up_disable_irq(priv->config->irq);
  esp32s3_teardown_irq(priv->cpu, priv->config->periph, priv->cpuint);
  priv->cpuint = -ENOMEM;

  spislave_deinitialize(ctrlr);

  spin_unlock_irqrestore(&priv->lock, flags);

  return OK;
}

#endif /* defined(CONFIG_ESP32S3_SPI) && defined (CONFIG_SPI_SLAVE) */
