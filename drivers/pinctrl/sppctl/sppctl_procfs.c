#include "sppctl_procfs.h"

static int dv_pf_open( struct inode *_i, struct file *_f) {  return( 0);  }
static int dv_pf_release(struct inode *_i, struct file *_f) {  return( 0);  }
static int dv_pf_W( struct file *_f, const char __user *_buf, size_t _cnt, loff_t *_l) {
 struct platform_device *_pdev = PDE_DATA( file_inode( _f));
 sppctl_pdata_t *p = ( sppctl_pdata_t *)_pdev->dev.platform_data;
 unsigned char buff[ 8 + 1];
 uint16_t pin = 0;
 if ( _cnt < 4) return( -EFAULT);
 if ( copy_from_user( buff, _buf,  ( _cnt >= 8 ? 8 : _cnt))) return( -EFAULT);
 pin = simple_strtoul( buff, NULL, 16);
 KINF( "%s():%d\n", __FUNCTION__, pin);
 // 0 is pin #
 sppctl_pin_set( p, pin, 8);
 return( _cnt);  }

static int dv_pf_R( struct file *_f, char __user *_buf, size_t _s, loff_t *_l) {
 struct platform_device *_pdev = PDE_DATA( file_inode( _f));
 sppctl_pdata_t *p = ( sppctl_pdata_t *)_pdev->dev.platform_data;
 unsigned char ret;
 unsigned char buff[ 8 + 1];
 uint32_t rval = 0x00080008;
 int i;
 if ( *_l > 0) return( 0);
 // 0 is pin #
 rval = sppctl_fun_get( p, 8);
 KINF( "%s():%d\n", __FUNCTION__, rval);
 memset( buff, 0, 8 + 1);
 for ( i = 0; i < 8; i++) {
   sprintf( buff + i*2, "%02X", (( unsigned char *)&rval)[ i]);
 }
 if ( copy_to_user( _buf, buff, 8)) return( -EFAULT);
 *_l += ret = 8;
 return( ret);  }

static struct file_operations fo_I;	// on/off

// ---------- main (exported) functions
void sppctl_procfs_init( struct platform_device *_pdev) {
 struct proc_dir_entry *tfs;
 // register procfs entry
 fo_I.owner = THIS_MODULE;
 fo_I.open = dv_pf_open;
 fo_I.llseek =  no_llseek;
 fo_I.release = dv_pf_release;
 fo_I.write = dv_pf_W;
 fo_I.read 	= dv_pf_R;
 tfs  = proc_create_data( dev_name( &( _pdev->dev)), 0, NULL, &fo_I, _pdev);
 return;  }

void sppctl_procfs_clean( struct platform_device *_pdev) {
 remove_proc_entry( dev_name( &( _pdev->dev)), NULL);
 return;  }
