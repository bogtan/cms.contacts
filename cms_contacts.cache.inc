<?php
/**
 * Author: Cristian Romanescu <cristi _at_ eaudeweb dot ro>
 * Created: 201401281231
 */

module_load_include('php', 'cms', 'CMSLDAP');

define('CMS_CONTACTS_CACHE_USERS', 'CMS_CONTACTS_CACHE_USERS');
define('CMS_CONTACTS_CACHE_INSTRUMENT_PEOPLE', 'CMS_CONTACTS_CACHE_INSTRUMENT_PEOPLE');
define('CMS_CONTACTS_CACHE_MAILINGLIST_PEOPLE', 'CMS_CONTACTS_CACHE_MAILINGLIST_PEOPLE');
define('CMS_CONTACTS_CACHE_STATUS_PERSON_PEOPLE', 'CMS_CONTACTS_CACHE_STATUS_PERSON_PEOPLE');
define('CMS_CONTACTS_CACHE_STATUS_ORGANIZATION_PEOPLE', 'CMS_CONTACTS_CACHE_STATUS_ORGANIZATION_PEOPLE');
define('CMS_CONTACTS_CACHE_ORGANIZATION', 'CMS_CONTACTS_CACHE_ORGANIZATION');
define('CMS_CONTACTS_CACHE_NFP_LISTS', 'CMS_CONTACTS_NFP_LISTS');
define('CMS_CONTACTS_CACHE_LIFETIME', 3600);

/**
 * Class CMSContactsCache handles LDAp contacts cache
 */
class CMSContactsCache {

    /** 
     * Clear the contacts LDAP cache. Next cache request will reload it
     */
    public static function cache_clear_all() {
        cache_clear_all(CMS_CONTACTS_CACHE_USERS, 'cache');
        cache_clear_all(CMS_CONTACTS_CACHE_INSTRUMENT_PEOPLE, 'cache');
        cache_clear_all(CMS_CONTACTS_CACHE_MAILINGLIST_PEOPLE, 'cache');
        cache_clear_all(CMS_CONTACTS_CACHE_STATUS_PERSON_PEOPLE, 'cache');
        cache_clear_all(CMS_CONTACTS_CACHE_STATUS_ORGANIZATION_PEOPLE, 'cache');
        cache_clear_all(CMS_CONTACTS_CACHE_NFP_LISTS, 'cache');
    }

    /**
     * Cache the people membership to the mailing lists
     * @return array Array of arrays keyed by mailinglist UUID with user DN as items
     */
    public static function cache_mailinglist_people() {
        $cache = cache_get(CMS_CONTACTS_CACHE_MAILINGLIST_PEOPLE);
        if(!$cache) {
            $ldap = CMSLDAP::get_instance();
            $filter = "(objectClass=" . CMS_MAILING. ")";
            $attributes = array('uuid', 'uniquemember');
            $ldap->search($ldap->base_dn, $filter, $attributes);
            $rows = $ldap->get_entries();
            $cache = array();
            unset($rows['count']);
            foreach ($rows as $row) {
                $uuid = $row['uuid'][0];
                $d = $row['uniquemember'];
                unset($d['count']);
                if(count($d) == 1 && $d[0] == '') {
                    continue;
                }
                if($d[0] == '') { unset($d[0]); }
                $cache[$uuid] = $d;
            }
            if ($cache) {
                cache_set(CMS_CONTACTS_CACHE_MAILINGLIST_PEOPLE, $cache, 'cache', time() + CMS_CONTACTS_CACHE_LIFETIME);
            }
        } else {
            $cache = $cache->data;
        }
        return $cache;
    }


    /**
     * Cache the people membership to statusPerson groups
     * @return array Array of arrays keyed by cmsStatusPerson DN with user DN as items
     */
    public static function cache_statusperson_people() {
        $cache = cache_get(CMS_CONTACTS_CACHE_STATUS_PERSON_PEOPLE);
        if(!$cache) {
            $ldap = CMSLDAP::get_instance();
            $filter = "(objectClass=" . CMS_STATUS_PERSON. ")";
            $attributes = array('uuid', 'uniquemember');
            $ldap->search($ldap->base_dn, $filter, $attributes);
            $rows = $ldap->get_entries();
            $cache = array();
            unset($rows['count']);
            foreach ($rows as $row) {
                $d = $row['uniquemember'];
                unset($d['count']);
                if(count($d) == 1 && $d[0] == '') {
                    continue;
                }
                if($d[0] == '') { unset($d[0]); }
                $dn = $row['dn'];
                $cache[$dn] = $d;
            }
            if ($cache) {
                cache_set(CMS_CONTACTS_CACHE_STATUS_PERSON_PEOPLE, $cache, 'cache', time() + CMS_CONTACTS_CACHE_LIFETIME);
            }
        } else {
            $cache = $cache->data;
        }
        return $cache;
    }


    /**
     * Cache the people membership to statusOrganization groups
     * @return array Array of arrays keyed by cmsStatusOrganization DN with user DN as items
     */
    public static function cache_statusorganization_people() {
        $cache = cache_get(CMS_CONTACTS_CACHE_STATUS_ORGANIZATION_PEOPLE);
        if(!$cache) {
            $ldap = CMSLDAP::get_instance();
            $filter = "(objectClass=" . CMS_STATUS_ORGANIZATION. ")";
            $attributes = array('uuid', 'uniquemember');
            $ldap->search($ldap->base_dn, $filter, $attributes);
            $rows = $ldap->get_entries();
            $cache = array();
            unset($rows['count']);
            foreach ($rows as $row) {
                $d = $row['uniquemember'];
                unset($d['count']);
                if(count($d) == 1 && $d[0] == '') {
                    continue;
                }
                if($d[0] == '') { unset($d[0]); }
                $dn = $row['dn'];
                $cache[$dn] = $d;
            }
            if ($cache) {
                cache_set(CMS_CONTACTS_CACHE_STATUS_ORGANIZATION_PEOPLE, $cache, 'cache', time() + CMS_CONTACTS_CACHE_LIFETIME);
            }
        } else {
            $cache = $cache->data;
        }
        return $cache;
    }


    /**
     * Cache people membership to instruments
     * @return array Array [instrument_uuid => (user1_dn, user2_dn)]
     */
    public static function cache_instrument_people() {
        $cache = cache_get(CMS_CONTACTS_CACHE_INSTRUMENT_PEOPLE);
        if(!$cache) {
            $ldap = CMSLDAP::get_instance();
            $filter = "(objectClass=" . CMS_INSTRUMENT . ")";
            $attributes = array('uuid', 'uniquemember');
            $ldap->search($ldap->base_dn, $filter, $attributes);
            $rows = $ldap->get_entries();
            unset($rows['count']);
            $cache = array();
            foreach ($rows as $row) {
                $uuid = $row['uuid'][0];
                $d = $row['uniquemember'];
                unset($d['count']);
                $cache[$uuid] = $d;
            }
            if (!empty($cache)) {
                # Create cache and keep it for 1 hour
                cache_set(CMS_CONTACTS_CACHE_INSTRUMENT_PEOPLE, $cache, 'cache', time() + CMS_CONTACTS_CACHE_LIFETIME);
            }
        } else {
            $cache = $cache->data;
        }
        return $cache;
    }


    /**
     * Cache the LDAP contacts.
     * !WARNING! This caches incomplete records. For performance reasions, only few LDAP attributes are loaded
     * @return array Array of stdClassess with selected LDAP attributes
     */
    public static function cache_ldap_users() {
        $cache = cache_get(CMS_CONTACTS_CACHE_USERS);
        if(!$cache) {
            $ldap = CMSLDAP::get_instance();
            $filter = "(objectClass=" . CMS_CONTACT . ")";
            $attributes = array(
                'uid', 'cn', 'sn', 'givenname', 'region', 'iso2', 'o', 'cpiso2', 'cmsavailability',
                'statusperson', 'statusorganization', 'mailinglists'
            );
            $true = 'TRUE';
            $ldap->search($ldap->base_dn, $filter, $attributes);
            $rows = $ldap->get_entries();
            $cache = array();

            $country_cache = Country::get_country_names_cached();
            $instrument_people = self::cache_instrument_people();
            $instruments = CMSLegalInstrument::list_keyed_by_uuid();
            unset($rows['count']);
            foreach($rows as $u) {
                $id = $u['dn'];
                $ob = new stdClass();
                $ob->city = '';
                $ob->dn = $id;
                $ob->uid = $u['uid'][0];
                $ob->sn = $u['sn'][0];
                $ob->cn = $u['cn'][0];
                $ob->givenname = $u['givenname'][0];
                $ob->view_link = sprintf('<a target="_blank" href="/%scontacts/item/%s/view">' . t('View') . '</a>', ADMINISTRATION_PATH, $ob->uid);
                $ob->edit_link = sprintf('<a target="_blank" href="/%scontacts/item/%s/edit">' . t('Edit') . '</a>', ADMINISTRATION_PATH, $ob->uid);
                $ob->organization = !empty($u['o'][0]) ? $u['o'][0] : '';

                $ob->region = array();
                if(array_key_exists('region', $u)) {
                    $tmp = $u['region'];
                    unset($tmp['count']);
                    $tmp = array_map('trim', explode(';', $tmp[0]));
                    $ob->region = $tmp;
                    $ob->region_rendered = $tmp[0];
                }

                $ob->countries = array();
                $ob->countries_rendered = '';
                if(array_key_exists('iso2', $u)) {
                    $tmp = $u['iso2'];
                    unset($tmp['count']);
                    $countries = array();
                    foreach($tmp as $iso2) {
                        if(isset($country_cache[$iso2])) {
                            $countries[] = $country_cache[$iso2];
                        }
                    }
                    $ob->iso2 = $tmp;
                    $ob->countries = $countries;
                    $ob->countries_rendered = implode(', ', $countries);
                }

                $ob->countries_post = array();
                $ob->countries_post_rendered = '';
                if(array_key_exists('cpiso2', $u)) {
                    $tmp = $u['cpiso2'];
                    unset($tmp['count']);
                    $countries = array();
                    foreach($tmp as $iso2) {
                        if(isset($country_cache[$iso2])) {
                            $countries[] = $country_cache[$iso2];
                        }
                    }
                    $ob->cpiso2 = $tmp;
                    $ob->countries_post = $countries;
                    $ob->countries_post_rendered = implode(', ', $countries);
                }

                $ob->availability = FALSE;
                if(array_key_exists('cmsavailability', $u)) {
                    $ob->availability = $u['cmsavailability'] == $true ? TRUE : FALSE;
                }

                $ob->statusperson = array();
                if(array_key_exists('statusperson', $u)) {
                    $tmp = $u['statusperson'];
                    unset($tmp['count']);
                    $ob->statusperson = $tmp;
                };

                $ob->statusorganization = array();
                if(array_key_exists('statusorganization', $u)) {
                    $tmp = $u['statusorganization'];
                    unset($tmp['count']);
                    $ob->statusorganization = $tmp;
                }

                $ob->mailinglists = array();
                if(array_key_exists('mailinglists', $u)) {
                    $tmp = $u['mailinglists'];
                    unset($tmp['count']);
                    $ob->mailinglists = $tmp;
                }


                # Compute the instruments
                $ob->instruments = array();
                foreach($instrument_people as $i_uuid => $group) {
                    if(in_array($id, $group)) {
                        $ob->instruments[$i_uuid] = $instruments[$i_uuid];
                    }
                }
                $ob->instruments_rendered = implode(',', $ob->instruments);

                # Add here all attributes required for filtering when using sSearch parameter
                $ob->filter = sprintf('%s %s %s %s %s %s %s',
                    $ob->givenname,
                    $ob->sn,
                    $ob->city,
                    $ob->countries_rendered,
                    $ob->dn,
                    $ob->organization,
                    $ob->instruments_rendered
                );

                $cache[$id] = $ob;
            }
            if (!empty($cache)) {
                # Create cache and keep it for 1 hour
                cache_set(CMS_CONTACTS_CACHE_USERS, $cache, 'cache', time() + CMS_CONTACTS_CACHE_LIFETIME);
            }
        } else {
            $cache = $cache->data;
        }
        return $cache;
    }


    /**
     * Retrieve the LDAP user names for using inside select options. If you want custom value use parameters.
     * @param string $key_attr (Optional) Which attribute holds the key. Default uid.
     * @param string $value_attr (Optional) Which attribute holds the values. Default cn.
     * @return array of strings with user uid as key
     */
    public static function get_ldap_users_names($key_attr = 'uid', $value_attr = 'cn') {
        $ret = array();
        $users = self::cache_ldap_users();
        foreach($users as $u) {
            if(isset($u->$key_attr) && isset($u->$value_attr)) {
                $ret[$u->$key_attr] = $u->$value_attr;
            } else {
                continue;
            }
        }
        return $ret;
    }


    /**
     * Retrieve the list of organizations.
     * !WARNING! This caches incomplete records. For performance reasions, only few LDAP attributes are loaded
     * @return array Array of stdClassess with selected LDAP attributes
     */
    public static function cache_organizations() {
        $cache = cache_get(CMS_CONTACTS_CACHE_ORGANIZATION);
        if(!$cache) {
            $ldap = CMSLDAP::get_instance();
            $filter = "(objectClass=" . CMS_ORGANIZATION . ")";
            $attributes = array(
                'dn', 'oid', 'o', 'cmsavailability', 'iso2'
            );
            $ldap->search($ldap->base_dn, $filter, $attributes);
            $rows = $ldap->get_entries();
            $cache = array();
            $true = "TRUE";
            unset($rows['count']);
            $country_cache = Country::get_country_names_cached();
            foreach($rows as $u) {
                $id = $u['dn'];
                $ob = new stdClass();
                $ob->dn = $id;
                $ob->oid = $u['oid'][0];
                $ob->cmsAvailability = $u['cmsavailability'][0] == $true;
                if(array_key_exists('iso2', $u)) {
                    $tmp = $u['iso2'];
                    unset($tmp['count']);
                    $countries = array();
                    foreach($tmp as $iso2) {
                        if(isset($country_cache[$iso2])) {
                            $countries[] = $country_cache[$iso2];
                        }
                    }
                    $ob->iso2 = $tmp;
                    $ob->countries = $countries;
                    $ob->countries_rendered = implode(', ', $countries);
                }
                $ob->o = $u['o'][0];
                $ob->name = $ob->o;

                $cache[$id] = $ob;
            }
        }
        return $cache;
    }

    public static function cache_nfp_lists() {
        $cache = cache_get(CMS_CONTACTS_CACHE_NFP_LISTS);
        if(!$cache) {
            $ldap = CMSLDAP::get_instance();
            $filter = "(objectClass=" . CMS_COUNTRY_NFP . ")";
            $attributes = array('dn', 'iso2', 'uuid', 'uniquemember');
            $ldap->search($ldap->base_dn, $filter, $attributes);
            $rows = $ldap->get_entries();
            unset($rows['count']);
            $cache = array();
            foreach ($rows as $row) {
                $ob = new stdClass();
                $dn = $row['dn'];
                unset($row['iso2']['count']);
                unset($row['uuid']['count']);
                unset($row['uniquemember']['count']);
                $ob->iso2 = $row['iso2'][0];
                $ob->uuid = $row['uuid'][0];
                $ob->uniquemember = $row['uniquemember'];
                $cache[$dn] = $ob;
            }
            if (!empty($cache)) {
                # Create cache and keep it for 1 hour
                cache_set(CMS_CONTACTS_CACHE_NFP_LISTS, $cache, 'cache', time() + CMS_CONTACTS_CACHE_LIFETIME);
            }
        } else {
            $cache = $cache->data;
        }
        return $cache;
    }
}
